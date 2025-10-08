#!/usr/bin/env zsh
set -euo pipefail

# ==== Config ====
DIR="/mnt/test"
LOGDIR="fio_logs"
JOBFILE="fio_scripts/basic.fio"   # [seqread][seqwrite][randread][randwrite] 섹션 존재
BSLIST=("4k" "128k")
NUMJOBS=(1 2 4 8 16 32)
JOBS=("read" "write" "randread" "randwrite")

# 선작성 크기 (환경변수로 조정 가능)
: ${PRE_SIZE:="1G"}

# 실행 시간(고정, no ramp)
RUN_TIME="30s"

DROP_CACHES='sync; echo 3 > /proc/sys/vm/drop_caches'

# ==== Helper: 레이아웃 선작성(섹션 미사용, 확정 옵션) ====
precreate_layout() {
  local job="$1"
  echo "[precreate] ${job} files @ ${DIR} size=${PRE_SIZE}"

  # 섹션/옵션 상속을 피하고, 실제 데이터가 할당되도록 순차 write + direct=0
  # numjobs=32로 0..31 모든 파일을 선작성해 이후 numjobs 조합을 커버
  sudo fio \
    --name="pre_${job}" \
    --directory="${DIR}" \
    --filename_format="${job}.\$jobnum.0" \
    --filesize="${PRE_SIZE}" \
    --rw=write \
    --bs=1M \
    --ioengine=psync \
    --direct=0 \
    --numjobs=32 \
    --output=/dev/null
}

# ==== Init ====
[[ -d "$DIR" ]] || { echo "ERROR: $DIR not found"; exit 1; }
mkdir -p "$LOGDIR"

# ==== Main loop ====
for job in "${JOBS[@]}"; do
  echo "========== SECTION BEGIN: ${job} ========="

  # 1) 레이아웃 선작성 (섹션 미사용)
  precreate_layout "$job"

  # 2) 쿨다운 (60초)
  echo "[wait] cool-down 60s..."
  sleep 60

  # 3) 실험 조합 실행
  for bs in "${BSLIST[@]}"; do
    for nj in "${NUMJOBS[@]}"; do
      out="${LOGDIR}/${job}_${bs}_${nj}.json"
      echo "==== RUN ${job} bs=${bs} numjobs=${nj} ===="

      # 캐시 드랍 후 잠시 대기
      sudo sh -c "$DROP_CACHES" || echo "[warn] drop_caches failed"
      sleep 10

      # 본 실험: 섹션 사용, direct 미사용(버퍼드), 시간 기반 30s, 랜덤 재현성 고정
      # directory/filename_format은 basic.fio에 이미 정의되어 있다면 생략 가능
      sudo fio "${JOBFILE}" \
        --section="${job}" \
        --bs="${bs}" \
        --numjobs="${nj}" \
        --time_based=1 \
        --runtime="${RUN_TIME}" \
        --randrepeat=1 --random_generator=lfsr --randseed=1 \
        --refill_buffers=1 --norandommap=1 \
        --group_reporting=1 \
        --eta=never \
        --output-format=json+ \
        --lat_percentiles=1 \
        --output="${out}"

      echo " -> saved: $out"
    done
  done
done

echo "[done] All evaluation completed."
