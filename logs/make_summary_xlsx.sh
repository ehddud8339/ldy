#!/usr/bin/env bash
set -euo pipefail

# 사용법:
#   ./make_summary_xlsx.sh TYPE LOG_DIR
# 예:
#   ./make_summary_xlsx.sh ycsb  /path/to/logs/ycsb/20251115_1630
#   ./make_summary_xlsx.sh fio   /path/to/logs/fio/20251115_1630
#   ./make_summary_xlsx.sh all   /path/to/logs/fio/20251115_1630

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 TYPE LOG_DIR"
  echo "  TYPE: ycsb | fio | filebench | all"
  exit 1
fi

TYPE="$1"
LOG_DIR="$2"

if [[ ! -d "${LOG_DIR}" ]]; then
  echo "[ERROR] LOG_DIR not found: ${LOG_DIR}"
  exit 1
fi

case "${TYPE}" in
ycsb)
  echo "[INFO] Generate YCSB summary only"
  sudo python3 ./ycsb_summary.py \
    --input "${LOG_DIR}" \
    --output "${LOG_DIR}/ycsb_summary.xlsx"
  ;;

fio)
  echo "[INFO] Generate FIO summary only"
  sudo python3 ./fio_summary.py \
    --input "${LOG_DIR}" \
    --output "${LOG_DIR}/fio_summary.xlsx"
  ;;

filebench)
  echo "[INFO] Generate FILEBENCH summary only"
  # filebench_summary.py가 준비되어 있다는 가정
  sudo python3 ./filebench_summary.py \
    --input "${LOG_DIR}" \
    --output "${LOG_DIR}/filebench_summary.xlsx"
  ;;

all)
  echo "[INFO] Generate ALL workload summaries (YCSB + FIO + FILEBENCH)"

  sudo python3 ./ycsb_summary.py \
    --input "${LOG_DIR}" \
    --output "${LOG_DIR}/ycsb_summary.xlsx" || true

  sudo python3 ./fio_summary.py \
    --input "${LOG_DIR}" \
    --output "${LOG_DIR}/fio_summary.xlsx" || true

  # filebench_summary.py가 없으면 여기서 에러 날 수 있으니 필요 없으면 삭제해도 됨
  sudo python3 ./filebench_summary.py \
    --input "${LOG_DIR}" \
    --output "${LOG_DIR}/filebench_summary.xlsx" || true
  ;;

*)
  echo "[ERROR] Unknown TYPE: ${TYPE}"
  echo "  TYPE must be one of: ycsb | fio | filebench | all"
  exit 1
  ;;
esac

# ==== 공통 metrics 요약 ====
# vmstat, cachestat, perf는 workload 종류와 관계 없이 동일하게 처리
if [[ -d "${LOG_DIR}/metrics" ]]; then
  echo "[INFO] Generate metrics summaries under ${LOG_DIR}/metrics"

  sudo python3 ./vmstat_xlsx_summary.py \
    --input "${LOG_DIR}/metrics" \
    --output "${LOG_DIR}/vmstat_summary.xlsx"

  sudo python3 ./cachestat_xlsx_summary.py \
    --input "${LOG_DIR}/metrics" \
    --output "${LOG_DIR}/cache_timeline.xlsx"

  sudo python3 ./perf_xlsx_summary.py \
    --input "${LOG_DIR}/metrics" \
    --output "${LOG_DIR}/perf_timeline.xlsx"
else
  echo "[WARN] Metrics directory not found: ${LOG_DIR}/metrics (skip vmstat/cachestat/perf summary)"
fi

echo "[DONE] Summary generation finished for TYPE=${TYPE}, LOG_DIR=${LOG_DIR}"
