#!/usr/bin/env bash

set -euo pipefail

python3 make_xlsx.py ./ext4 --out ext4_summary.xlsx
python3 make_xlsx.py ./fuse --out fuse_summary.xlsx
python3 make_xlsx.py ./rfuse --out rfuse_summary.xlsx
python3 make_xlsx.py ./rfuse_rr --out rfuse_rr_summary.xlsx
python3 make_xlsx.py ./rfuse_thr --out rfuse_thr_summary.xlsx
python3 make_xlsx.py ./rfuse_busy --out rfuse_busy_summary.xlsx

python3 merge_xlsx.py . --out merged_summary.xlsx
