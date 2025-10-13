
import os
import re
import sys
from pathlib import Path
from collections import defaultdict, OrderedDict

import pandas as pd
from openpyxl import Workbook
from openpyxl.utils import get_column_letter
from openpyxl.styles import Alignment, Font, Border, Side

# -------------- Helpers --------------

CPUS_WHITELIST = {"0,2,4,6", "8,10,12,14"}
FILENAME_RE = re.compile(
    r'^(?P<cpus>[\d,]+)_(?P<workload>[A-Za-z]+)_(?P<bs>\d+k)_(?P<numjobs>\d+)\.log$'
)

def _num_with_suffix_to_float(s: str) -> float:
    """Convert fio-style numeric with suffix k/M/G to float (no thousands separators)."""
    s = s.strip()
    m = re.match(r'^\s*([0-9]*\.?[0-9]+)\s*([kKmMgG]?)\s*$', s)
    if not m:
        # fallback: try int
        try:
            return float(s)
        except Exception:
            return float("nan")
    val = float(m.group(1))
    suf = m.group(2).lower()
    if suf == 'k':
        val *= 1e3
    elif suf == 'm':
        val *= 1e6
    elif suf == 'g':
        val *= 1e9
    return val

def parse_fio_file_metrics(path: Path) -> dict:
    """
    Parse a fio output log to extract metrics:
    - IOPS (numeric)
    - BW_MBps (MB/s, decimal megabytes per second)
    - LAT_AVG_USEC (avg 'lat' in microseconds)
    Returns dict; missing values are set to None.
    """
    text = path.read_text(errors="ignore")

    # IOPS
    iops = None
    m = re.search(r'IOPS\s*=\s*([0-9\.]+[kKmMgG]?)', text)
    if m:
        iops = _num_with_suffix_to_float(m.group(1))

    # BW in MB/s: Prefer value inside parentheses like "(2057MB/s)"
    bw_mb = None
    m = re.search(r'\(\s*([0-9]*\.?[0-9]+)\s*MB/s\s*\)', text)
    if m:
        bw_mb = float(m.group(1))
    else:
        # Fallback: parse "BW=1962MiB/s" and convert MiB/s -> MB/s (MiB * 1.048576 = MB)
        m2 = re.search(r'BW\s*=\s*([0-9]*\.?[0-9]+)\s*MiB/s', text, re.IGNORECASE)
        if m2:
            mib = float(m2.group(1))
            bw_mb = mib * 1.048576  # 1 MiB = 1.048576 MB

    # lat avg: Prefer "lat (" total latency), else fall back to clat
    lat_avg_usec = None
    m = re.search(r'lat\s*\(\s*(nsec|usec|msec)\s*\)\s*:\s*.*?avg\s*=\s*([0-9]*\.?[0-9]+)',
                  text, re.IGNORECASE | re.DOTALL)
    if not m:
        m = re.search(r'clat\s*\(\s*(nsec|usec|msec)\s*\)\s*:\s*.*?avg\s*=\s*([0-9]*\.?[0-9]+)',
                      text, re.IGNORECASE | re.DOTALL)

    if m:
        unit = m.group(1).lower()
        avg = float(m.group(2))
        if unit == "nsec":
            lat_avg_usec = avg / 1000.0
        elif unit == "usec":
            lat_avg_usec = avg
        elif unit == "msec":
            lat_avg_usec = avg * 1000.0

    return {
        "IOPS": iops,
        "BW(MB/s)": bw_mb,
        "lat_avg": lat_avg_usec,
    }

def build_group_key(cpus: str, workload: str, bs: str) -> str:
    return f"{cpus}_{workload}_{bs}"

# -------------- Main --------------

def main(log_dir: str = "."):
    base = Path(log_dir).resolve()
    logs = sorted(p for p in base.glob("*.log") if p.is_file())

    if not logs:
        print(f"[INFO] No .log files found in: {base}")
        return 0

    grouped: dict[str, dict[int, dict]] = defaultdict(dict)

    for p in logs:
        m = FILENAME_RE.match(p.name)
        if not m:
            # Skip unmatched filenames silently
            continue
        cpus = m.group("cpus")
        workload = m.group("workload")
        bs = m.group("bs")
        numjobs = int(m.group("numjobs"))

        # Only consider two cpus sets
        if cpus not in CPUS_WHITELIST:
            continue

        key = build_group_key(cpus, workload, bs)
        metrics = parse_fio_file_metrics(p)
        grouped[key][numjobs] = metrics

    if not grouped:
        print("[INFO] No valid groups (matching cpus whitelist and filename pattern) found.")
        return 0

    # Prepare Excel workbook
    wb = Workbook()
    # Remove default sheet
    wb.remove(wb.active)

    # Styles
    center = Alignment(horizontal="center", vertical="center")
    right = Alignment(horizontal="right", vertical="center")
    bold = Font(bold=True)
    thin = Side(style="thin", color="000000")
    border_all = Border(left=thin, right=thin, top=thin, bottom=thin)

    # For each group create a sheet
    for key, by_jobs in grouped.items():
        ws = wb.create_sheet(title=key[:31])  # Excel sheet name limit
        # Header merge: B1..E1 merged with key
        ws.merge_cells("B1:E1")
        ws["B1"].value = key
        ws["B1"].font = bold
        ws["B1"].alignment = center

        # Column headers (numjobs 1..4)
        for idx, nj in enumerate([1, 2, 3, 4], start=2):  # B..E
            cell = ws.cell(row=2, column=idx, value=nj)
            cell.font = bold
            cell.alignment = center
            cell.border = border_all

        # Row labels
        rows = ["IOPS", "BW(MB/s)", "lat_avg"]
        for r_i, label in enumerate(rows, start=3):
            lab_cell = ws.cell(row=r_i, column=1, value=label)
            lab_cell.font = bold
            lab_cell.alignment = right
            lab_cell.border = border_all

            # Fill metrics per numjobs
            for c_i, nj in enumerate([1, 2, 3, 4], start=2):
                val = None
                if nj in by_jobs and label in by_jobs[nj]:
                    val = by_jobs[nj][label]
                cell = ws.cell(row=r_i, column=c_i, value=val)
                cell.alignment = center
                cell.border = border_all

        # Set column widths
        ws.column_dimensions["A"].width = 14
        for col in ["B","C","D","E"]:
            ws.column_dimensions[col].width = 12

    out_name = f"{base.name}_summary.xlsx"
    out_path = base / out_name
    wb.save(out_path)
    print(f"[OK] Wrote summary workbook: {out_path}")

if __name__ == "__main__":
    log_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    main(log_dir)

