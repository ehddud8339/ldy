#!/bin/bash

set -euo pipefail

# ===== User Config =====
FS_TYPE=("ext4" "fuse" "rfuse")
#FS_TYPE=("rfuse")
FS_PATH="../../filesystems/stackfs"
DEVICE_NAME=("/dev/nvme1n1")

MOUNT_BASE="/mnt/RFUSE_EXT4"
MOUNT_POINT="/mnt/test"

SECTIONS=("read" "write" "randread" "randwrite")
BS_LIST=("4k" "128k")
NUM_JOBS=("1" "2" "3" "4")
BUSY_SET=("0" "2" "4" "8")
IDLE_SET=("8" "10" "12" "14")
FIO_BUSY_BASE="fio_scripts/busy.fio"
FIO_IDLE_BASE="fio_scripts/idle.fio"
RESULT_DIR="/home/ldy/src/ldy/rfuse/"

# ===== Helpers =====
function drop_all_caches() {
  sudo sh -c 'sync'
  sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
}

# ===== Mount helpers (from your base) =====
function init_mount_point() {
  local fs=$1
  local dev=$2

  drop_all_caches

  if [ "${fs}" == "ext4" ]; then
    set +e
    echo "Unmount ext4..."
    sudo umount "${MOUNT_POINT}" 2>/dev/null
    set -e

    echo "Mount ext4 (mkfs fresh)..."
    sudo mkfs.ext4 -F -E lazy_itable_init=0,lazy_journal_init=0 "${dev}"
    sudo mount "${dev}" "${MOUNT_POINT}"
    sudo sync

  elif [ "${fs}" == "fuse" ]; then
    set +e
    echo "Unmount fuse-stackfs..."
    sudo umount "${MOUNT_POINT}" 2>/dev/null

    echo "Unmount ext4..."
    sudo umount "${MOUNT_BASE}" 2>/dev/null
    set -e

    echo "Mount ext4 (mkfs fresh for lower fs)..."
    sudo mkfs.ext4 -F -E lazy_itable_init=0,lazy_journal_init=0 "${dev}"
    sudo mount "${dev}" "${MOUNT_BASE}"

    echo "Mount fuse-stackfs..."
    pushd "${FS_PATH}" >/dev/null
    cp /home/ldy/src/rfuse/filesystems/stackfs/StackFS_LowLevel.c.fuse /home/ldy/src/rfuse/filesystems/stackfs/StackFS_LowLevel.c
    make clean
    make
    ./StackFS_ll -r "${MOUNT_BASE}" "${MOUNT_POINT}" &
    popd >/dev/null
    sudo sync

  elif [ "${fs}" == "rfuse" ]; then
    set +e
    echo "Unmount rfuse-stackfs..."
    sudo umount "${MOUNT_POINT}" 2>/dev/null

    echo "Unmount ext4..."
    sudo umount "${MOUNT_BASE}" 2>/dev/null
    set -e

    echo "Mount ext4 (mkfs fresh for lower fs)..."
    sudo mkfs.ext4 -F -E lazy_itable_init=0,lazy_journal_init=0 "${dev}"
    sudo mount "${dev}" "${MOUNT_BASE}"

    echo "Mount rfuse-stackfs..."
    pushd "${FS_PATH}" >/dev/null
    cp /home/ldy/src/rfuse/filesystems/stackfs/StackFS_LowLevel.c.rfuse /home/ldy/src/rfuse/filesystems/stackfs/StackFS_LowLevel.c
    make clean
    make
    ./StackFS_ll -r "${MOUNT_BASE}" "${MOUNT_POINT}" &
    popd >/dev/null
    sudo sync

  else
    echo "Wrong file system type: ( ext4 | fuse | rfuse )"
    exit 1
  fi

  echo "Waiting Initialization..."
  sleep 60
}

function remount_point() {
  local fs=$1
  local dev=$2

  drop_all_caches

  if [ "${fs}" == "ext4" ]; then
    set +e
    echo "Unmount ext4..."
    sudo umount "${MOUNT_POINT}" 2>/dev/null
    set -e

    echo "Mount ext4..."
    sudo mount "${dev}" "${MOUNT_POINT}"
    sudo sync

  elif [ "${fs}" == "fuse" ]; then
    set +e
    echo "Unmount fuse-stackfs..."
    sudo umount "${MOUNT_POINT}" 2>/dev/null

    echo "Unmount ext4..."
    sudo umount "${MOUNT_BASE}" 2>/dev/null
    set -e

    echo "Mount ext4..."
    sudo mount "${dev}" "${MOUNT_BASE}"

    echo "Mount fuse-stackfs..."
    pushd "${FS_PATH}" >/dev/null
    cp /home/ldy/src/rfuse/filesystems/stackfs/StackFS_LowLevel.c.fuse /home/ldy/src/rfuse/filesystems/stackfs/StackFS_LowLevel.c
    make clean
    make
    ./StackFS_ll -r "${MOUNT_BASE}" "${MOUNT_POINT}" &
    popd >/dev/null
    sudo sync

  elif [ "${fs}" == "rfuse" ]; then
    set +e
    echo "Unmount rfuse-stackfs..."
    sudo umount "${MOUNT_POINT}" 2>/dev/null

    echo "Unmount ext4..."
    sudo umount "${MOUNT_BASE}" 2>/dev/null
    set -e

    echo "Mount ext4..."
    sudo mount "${dev}" "${MOUNT_BASE}"

    echo "Mount rfuse-stackfs..."
    pushd "${FS_PATH}" >/dev/null
    cp /home/ldy/src/rfuse/filesystems/stackfs/StackFS_LowLevel.c.rfuse /home/ldy/src/rfuse/filesystems/stackfs/StackFS_LowLevel.c
    make clean
    make
    ./StackFS_ll -r "${MOUNT_BASE}" "${MOUNT_POINT}" &
    popd >/dev/null
    sudo sync

  else
    echo "Wront file system type: ( ext4 | fuse | rfuse )"
    exit 1
  fi

  echo "Waiting Initialization..."
  sleep 60
}

function change_driver() {
  local fs=$1

  drop_all_caches

  if [ "${fs}" == "ext4" ]; then
    echo "ext4 doesn't need driver"

  elif [ "${fs}" == "fuse" ]; then
    echo "Change to fuse driver"
    pushd "../../lib/libfuse" >/dev/null
    ./libfuse_install.sh
    popd >/dev/null

    pushd "../../driver/fuse" >/dev/null
    ./fuse_insmod.sh
    popd >/dev/null

  elif [ "${fs}" == "rfuse" ]; then
    echo "Change to rfuse driver"
    pushd "../../lib/librfuse" >/dev/null
    ./librfuse_install.sh
    popd >/dev/null

    pushd "../../driver/rfuse" >/dev/null
    ./rfuse_insmod.sh
    popd >/dev/null

  else
    echo "Wront file system type: ( ext4 | fuse | rfuse )"
    exit 1
  fi
}

# ===== FIO runners =====
# 특정 CPU 코어에 부하를 높이기 위한 백그라운드 FIO
function bg_load_on_cpus() {
  echo "[BG] Run on CPUs[${BUSY_SET}]"

  fio "${FIO_BUSY_BASE}" \
    --directory="${MOUNT_POINT}" \
    --section="randwrite" \
    --bs=128k --size=4G --numjobs=4 \
    --cpus_allowed=${BUSY_SET} --cpus_allowed_policy=split \
    --output=/dev/null &
  BG_PID=$!
  sleep "[BG] running... (pid=${BG_PID}"
}

# section 반복 전, 해당 section의 최대 numjobs를 실행하여 파일을 생성 (프리필)
# - read/randread의 경우에도 실제 파일 생성이 필요하므로 --rw=write로 오버라이드
function prefill_for_section() {
  local fs=$1
  local section=$2
  local max_nj="${NUM_JOBS[$((${#NUM_JOBS[@]} - 1))]}" # 마지막 요소 = 최대 numjobs
  local outdir=$3

  echo "[${fs}] Prefill for section=${section} (numjobs=${max_nj})"

  drop_all_caches

  # 프리필은 빠르게 끝내기 위해 큰 bs 사용(128k), rw=write로 강제
  fio "${FIO_IDLE_BASE}" \
    --directory="${MOUNT_POINT}" \
    --section="${section}" \
    --rw=write \
    --bs=1M \
    --numjobs="${max_nj}" \
    --eta=never \
    --output="/dev/null"
}

# 실험 1회 실행 (로그 파일명은 [section]_[bs]_[numjobs].log 로 저장)
function run_one_fio() {
  local section=$1
  local bs=$2
  local nj=$3
  local cpus=$4
  local outdir=$5

  drop_all_caches

  local outfile="${outdir}/${section}_${bs}_${nj}.log"
  echo "Run: section=${section}, bs=${bs}, numjobs=${nj} -> ${outfile}"

  fio "${FIO_IDLE_BASE}" \
    --directory="${MOUNT_POINT}" \
    --section="${section}" \
    --bs="${bs}" \
    --numjobs="${nj}" \
    --cpus_allowed=${cpus} --cpus_allowed_policy=split \
    --eta=never \
    --output="${outfile}"
}

# ===== Main =====
mkdir -p "${RESULT_DIR}"

# (FS) → (section → prefill → (bs → (numjobs))) → next FS   # 1.4
for fs in "${FS_TYPE[@]}"; do
  echo "==== FS: ${fs} ===="
  change_driver "${fs}"
  init_mount_point "${fs}" "${DEVICE_NAME[0]}"

  # FS 별 결과 디렉토리
  fs_outdir="${RESULT_DIR}/${fs}"
  mkdir -p "${fs_outdir}"

  bg_load_on_cpus

  for section in "${SECTIONS[@]}"; do
    echo "== Section: ${section} =="

    # 1.5 섹션 반복 전 프리필(최대 numjobs로 파일 생성)
    prefill_for_section "${fs}" "${section}" "${fs_outdir}"

    # 1.4 섹션 반복 안에서 (bs → numjobs) 순서
    # numjobs에 따라서 BUSY_SET, IDLE_SET을 넘겨줘는 로직이 필요함
    for bs in "${BS_LIST[@]}"; do
      for nj in "${[@]}"; do
        run_one_fio "${section}" "${bs}" "${nj}" "${}" "${fs_outdir}"
      done
    done
  done

  echo "==== FS done: ${fs} ===="
  echo "[BG] stopping background load (pid=${BG_PID}"
  kill ${BG_PID} >/dev/null 2>&1 || true
  wait ${BG_PID} 2>/dev/null || true
done

echo "All experiments completed. Results in: ${RESULT_DIR}"
