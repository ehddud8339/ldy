#!/usr/bin/env bash

set -euo pipefail

#python3 make_xlsx.py ./ext4 --out ext4_summary.xlsx
#python3 make_xlsx.py ./fuse --out fuse_summary.xlsx
#python3 make_xlsx.py ./rfuse --out rfuse_summary.xlsx
#python3 make_xlsx.py ./rfuse_rr --out rfuse_rr_summary.xlsx
#python3 make_xlsx.py ./rfuse_thr --out rfuse_thr_summary.xlsx
#python3 make_xlsx.py ./rfuse --out rfuse_summary.xlsx
#python3 make_xlsx.py ./rfuse_busy_5 --out rfuse_busy_5_summary.xlsx
#python3 make_xlsx.py ./rfuse_busy_10 --out rfuse_busy_10_summary.xlsx
#python3 make_xlsx.py ./rfuse_busy_20 --out rfuse_busy_20_summary.xlsx
#python3 make_xlsx.py ./rfuse_busy_40 --out rfuse_busy_40_summary.xlsx
python3 make_xlsx.py ./fuse --out rfuse_summary.xlsx
python3 make_xlsx.py ./fuse_busy_5 --out rfuse_busy_5_summary.xlsx
python3 make_xlsx.py ./fuse_busy_10 --out rfuse_busy_10_summary.xlsx
python3 make_xlsx.py ./fuse_busy_20 --out rfuse_busy_20_summary.xlsx
python3 make_xlsx.py ./fuse_busy_40 --out rfuse_busy_40_summary.xlsx
#python3 make_xlsx.py ./fuse_Ecore --out rfuse_Ecore_summary.xlsx

python3 merge_xlsx.py . --out merged_summary.xlsx
