#!/usr/bin/env bash

set -euo pipefail

#python3 make_xlsx.py ./ext4 --out ext4_summary.xlsx
#python3 make_xlsx.py ./fuse --out fuse_summary.xlsx
#python3 make_xlsx.py ./rfuse --out rfuse_summary.xlsx
#python3 make_xlsx.py ./rfuse_rr --out rfuse_rr_summary.xlsx
#python3 make_xlsx.py ./rfuse_thr --out rfuse_thr_summary.xlsx
python3 make_xlsx.py ./rfuse --out rfuse_summary.xlsx
python3 make_xlsx.py ./rfuse_busy_2 --out rfuse_busy_2_summary.xlsx
python3 make_xlsx.py ./rfuse_busy_6 --out rfuse_busy_6_summary.xlsx
python3 make_xlsx.py ./rfuse_busy_12 --out rfuse_busy_12_summary.xlsx
python3 make_xlsx.py ./rfuse_busy_24 --out rfuse_busy_24_summary.xlsx

#python3 make_xlsx.py ./rfuse_Ecore --out rfuse_Ecore_summary.xlsx

python3 merge_xlsx.py . --out merged_summary.xlsx
