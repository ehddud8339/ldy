#!/usr/bin/env bash
set -euo pipefail

LOG_DIR=$1

sudo python3 ./ycsb_summary.py --input ${LOG_DIR} --output "${LOG_DIR}/ycsb_summary.xlsx"

sudo python3 ./vmstat_xlsx_summary.py --input "${LOG_DIR}/metrics" --output "${LOG_DIR}/vmstat_summary.xlsx"
