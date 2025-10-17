#!/usr/bin/env bash

pipe set -euo

python3 make_xlsx.py ./ext4 --out ext4_summary.xlsx
python3 make_xlsx.py ./fuse --out fuse_summary.xlsx
python3 make_xlsx.py ./rfuse --out rfuse_summary.xlsx

python3 merge_xlsx.py . --out merged_summary.xlsx
