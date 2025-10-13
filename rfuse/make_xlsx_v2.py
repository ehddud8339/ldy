import os
import re
import sys
from pathlib import Path
from collections import defaultdict

import pandas as pd
from openpyxl import Workbook
from openpyxl.styles import Alignment, Font, Border, Side


FILENAME_RE = re.compile(
    r'^(?P<cpus>[\d,]+)_(?P<workload>[A-Za-z]+)_(?P<bs>\d+k)_(?P<numjobs>\d+)\.log$'
)

def _num_with_suffix_to_float(s: str) -> float:
    s = s.strip()
    m = re.match(r'([0-9]*\.?[0-9]+)\s*([kKmMgG]?)', s)
    if not m:
        return float("nan")
    val, suf = float(m.group(1)), m.group(2).lower()
    return val * {"k": 1e3, "m": 1e6, "g": 1e9}.get(suf, 1)


def parse_fio_file(path: Path) -> dict:
    text = path.read_text(errors="ignore")

    # IOPS
    iops = None
    if m := re.search(r"IOPS\s*=\s*([0-9\.]+[kKmMgG]?)", text):
        iops = _num_with_suffix_to_float(m.group(1))

    # BW(MB/s)
    bw = None
    if m := re.search(r"\(([0-9\.]+)\s*MB/s\)", text):
        bw = float(m.group(1))
    elif m := re.search(r"BW\s*=\s*([0-9\.]+)\s*MiB/s", text, re.I):
        bw = float(m.group(1)) * 1.048576

    # lat_avg
    lat_avg = None
    if m := re.search(r"lat\s*\((nsec|usec|msec)\).*?avg\s*=\s*([0-9\.]+)", text, re.S | re.I):
        unit, val = m.group(1).lower(), float(m.group(2))
    elif m := re.search(r"clat\s*\((nsec|usec|msec)\).*?avg\s*=\s*([0-9\.]+)", text, re.S | re.I):
        unit, val = m.group(1).lower(), float(m.group(2))
    else:
        unit, val = None, None
    if unit:
        if unit == "nsec": lat_avg = val / 1000
        elif unit == "usec": lat_avg = val
        elif unit == "msec": lat_avg = val * 1000

    return {"IOPS": iops, "BW(MB/s)": bw, "lat_avg": lat_avg}


def main(log_dir=".", output_path=None):
    base = Path(log_dir).resolve()
    logs = [p for p in base.glob("*.log") if FILENAME_RE.match(p.name)]
    if not logs:
        print(f"[WARN] No .log files found in {base}")
        return

    # Parse all files
    parsed = []
    for p in logs:
        m = FILENAME_RE.match(p.name)
        d = parse_fio_file(p)
        d.update(m.groupdict())
        d["numjobs"] = int(d["numjobs"])
        parsed.append(d)

    df = pd.DataFrame(parsed)

    # Build unique (cpus, workload, bs) groups sorted
    cpus_groups = sorted(df["cpus"].unique(), key=lambda x: [int(i) for i in x.split(",")])
    workloads = sorted(df["workload"].unique())
    bs_list = sorted(df["bs"].unique(), key=lambda x: int(x.replace("k", "")))

    # Excel setup
    wb = Workbook()
    ws = wb.active
    ws.title = "summary"

    center = Alignment(horizontal="center", vertical="center")
    right = Alignment(horizontal="right", vertical="center")
    bold = Font(bold=True)
    border = Border(*(Side(style="thin", color="000000"),)*4)

    current_row = 1
    written_tables = 0

    # target 16 combinations (2 cpus groups × 4 workloads × 2 bs)
    target_cpu_sets = ["0,2,4,8", "8,10,12,14"]
    for cpus in target_cpu_sets:
        for workload in workloads:
            for bs in bs_list:
                # Determine required incremental sets
                sub_cpu_sets = [",".join(map(str, list(range(0, step+2, 2)))) for step in range(0, 8, 2)]
                sub_cpu_sets = sub_cpu_sets[:4]  # up to 4 numjobs

                rows = ["IOPS", "BW(MB/s)", "lat_avg"]
                # Header merge
                ws.merge_cells(start_row=current_row, start_column=2, end_row=current_row, end_column=5)
                ws.cell(current_row, 2).value = f"{cpus}_{workload}_{bs}"
                ws.cell(current_row, 2).font = bold
                ws.cell(current_row, 2).alignment = center
                current_row += 1

                # numjobs header
                for i, nj in enumerate([1, 2, 3, 4], start=2):
                    c = ws.cell(current_row, i, nj)
                    c.font = bold; c.alignment = center; c.border = border
                current_row += 1

                # rows (IOPS, BW, lat_avg)
                for r_name in rows:
                    ws.cell(current_row, 1, r_name).font = bold
                    ws.cell(current_row, 1).alignment = right
                    ws.cell(current_row, 1).border = border

                    for i, nj in enumerate([1, 2, 3, 4], start=2):
                        if nj <= len(sub_cpu_sets):
                            subset = sub_cpu_sets[nj - 1]
                            subdf = df[
                                (df["cpus"] == subset)
                                & (df["workload"] == workload)
                                & (df["bs"] == bs)
                                & (df["numjobs"] == nj)
                            ]
                            val = subdf[r_name].iloc[0] if not subdf.empty else None
                        else:
                            val = None
                        c = ws.cell(current_row, i, val)
                        c.alignment = center; c.border = border
                    current_row += 1

                # spacing
                current_row += 3
                written_tables += 1

    out = Path(output_path) if output_path else base / f"{base.name}_summary.xlsx"
    wb.save(out)
    print(f"[OK] Created {written_tables} tables → {out}")

if __name__ == "__main__":
    log_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    output_path = sys.argv[2] if len(sys.argv) > 2 else None
    main(log_dir, output_path)

