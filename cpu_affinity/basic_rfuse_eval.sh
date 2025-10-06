#!/usr/bin/env zsh
set -euo pipefail

# ==== Config ====
DIR="/mnt/test"
LOGDIR="fio_logs"
JOBFILE="sync.fio" # 섹션 정의된 fio jobfile (seqread/seqwrite/randread/randwrite)

# 가변 파라미터
BSLIST=("4k" "128k")
NUMJOBS=(1 2 4 8 16 32)
JOBS=("seqread" "seqwrite" "randread" "randwrite")

# 선작성 크기 (작은 값 권장; 환경변수로 조정 가능)
: ${PRE_SIZE:="4G"}        # export PRE_SIZE=256M
: ${RUN_SIZE:="$PRE_SIZE"} # 실험 size도 동일/작게

DROP_CACHES='sync; echo 3 > /proc/sys/vm/drop_caches'

# ==== Helper ====
delete_files() {
  local job="$1"
  echo "[cleanup] Removing files for section ${job} in ${DIR}"
  # zsh nomatch 회피: (N) 한정자 사용
  rm -f -- "${DIR}"/${job}.*.*(N) 2>/dev/null || true
}

precreate_layout() {
  local job="$1"
  echo "[precreate] Create files for section ${job} in ${DIR} (PRE_SIZE=${PRE_SIZE})"
  # 32개 파일을 한 번에 만들어두면 0..31 파일이 모두 생성되어 numjobs 조합을 커버
  fio "${JOBFILE}" \
    --section="${job}" \
    --rw=write \
    --bs=1M \
    --size="${PRE_SIZE}" \
    --numjobs=32 \
    --output=/dev/null
}

# ==== Init ====
[[ -d "$DIR" ]] || {
  echo "ERROR: $DIR not found"
  exit 1
}
mkdir -p "$LOGDIR"

# ==== Main loop ====
for job in "${JOBS[@]}"; do
  echo "========== SECTION BEGIN: ${job} ========="
  # 이전 잔재 제거(안전)
  delete_files "$job"

  # 1) 섹션 파일 선작성 (레이아웃 고정)
  precreate_layout "$job"

  # 2) 조합 실행
  for bs in "${BSLIST[@]}"; do
    for nj in "${NUMJOBS[@]}"; do
      out="${LOGDIR}/${job}_${bs}_${nj}.json"

      echo "==== RUN ${job} bs=${bs} numjobs=${nj} ===="

      # 캐시 드랍 + 대기
      sudo sh -c "$DROP_CACHES" || echo "[warn] drop_caches failed"
      sleep 5

      # 실행 (directory 모드; filename 미사용 → 파일 contention 회피)
      sudo fio "${JOBFILE}" \
        --section="${job}" \
        --bs="${bs}" \
        --numjobs="${nj}" \
        --size="${RUN_SIZE}" \
        --output-format=json \
        --output="${out}"

      echo " -> saved: $out"
    done
  done

  # 3) 섹션 완료 후 파일 삭제 + 30초 대기
  delete_files "$job"
  echo "[wait] 30 sec cool-down..."
  sleep 30
done

echo "[done] All evaluation completed."
