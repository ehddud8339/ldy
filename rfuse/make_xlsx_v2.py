
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

    iops = None
    m = re.search(r"IOPS\s*=\s*([0-9\.]+[kKmMgG]?)", text)
    if m: iops = _num_with_suffix_to_float(m.group(1))

    bw = None
    m = re.search(r"\(([0-9\.]+)\s*MB/s\)", text)
    if m: bw = float(m.group(1))
    else:
        m = re.search(r"BW\s*=\s*([0-9\.]+)\s*MiB/s", text, re.I)
        if m: bw = float(m.group(1)) * 1.048576

    lat_avg = None
    m = re.search(r"lat\s*\((nsec|usec|msec)\).*?avg\s*=\s*([0-9\.]+)", text, re.S | re.I) \
        or re.search(r"clat\s*\((nsec|usec|msec)\).*?avg\s*=\s*([0-9\.]+)", text, re.S | re.I)
    if m:
        unit, val = m.group(1).lower(), float(m.group(2))
        lat_avg = val/1000 if unit=="nsec" else val if unit=="usec" else val*1000

    return {"IOPS": iops, "BW(MB/s)": bw, "lat_avg": lat_avg}

def bs_key(bs: str):
    m = re.match(r"(\d+)\s*k$", bs.strip(), re.I)
    return int(m.group(1)) if m else 10**9

def workload_key(w):
    order = ["randread", "randwrite", "read", "write"]
    return order.index(w) if w in order else len(order)

def main(log_dir=".", output_path=None):
    base = Path(log_dir).resolve()
    logs = [p for p in base.glob("*.log") if FILENAME_RE.match(p.name)]
    if not logs:
        print(f"[WARN] No .log files found in {base}")
        return

    parsed = []
    for p in logs:
        m = FILENAME_RE.match(p.name)
        d = parse_fio_file(p)
        d.update(m.groupdict())
        d["numjobs"] = int(d["numjobs"])
        parsed.append(d)

    df = pd.DataFrame(parsed)

    # Canonical target groups we care about
    target_cpu_sets = []
    for cg in ["0,2,4,8", "8,10,12,14"]:
        if cg in df["cpus"].unique():
            target_cpu_sets.append(cg)

    workloads = sorted(df["workload"].unique(), key=workload_key)
    bs_list = sorted(df["bs"].unique(), key=bs_key)

    from openpyxl import Workbook
    wb = Workbook()
    ws = wb.active
    ws.title = "summary"

    center = Alignment(horizontal="center", vertical="center")
    right = Alignment(horizontal="right", vertical="center")
    bold = Font(bold=True)
    thin = Side(style="thin", color="000000")
    border = Border(left=thin, right=thin, top=thin, bottom=thin)

    current_row = 1
    written_tables = 0

    def prefixes(cg: str, upto: int):
        parts = cg.split(",")
        out = []
        for k in range(1, upto+1):
            out.append(",".join(parts[:k]))
        return out

    for cpus in target_cpu_sets:
        for workload in workloads:
            for bs in bs_list:
                sub_cpu_sets = prefixes(cpus, 4)

                ws.merge_cells(start_row=current_row, start_column=2, end_row=current_row, end_column=5)
                ws.cell(current_row, 2, f"{cpus}_{workload}_{bs}").font = bold
                ws.cell(current_row, 2).alignment = center
                current_row += 1

                for i, nj in enumerate([1,2,3,4], start=2):
                    c = ws.cell(current_row, i, nj)
                    c.font = bold; c.alignment = center; c.border = border
                current_row += 1

                for r_name in ["IOPS", "BW(MB/s)", "lat_avg"]:
                    ws.cell(current_row, 1, r_name).font = bold
                    ws.cell(current_row, 1).alignment = right
                    ws.cell(current_row, 1).border = border

                    for i, nj in enumerate([1,2,3,4], start=2):
                        subset = sub_cpu_sets[nj - 1]
                        subdf = df[
                            (df["cpus"] == subset) &
                            (df["workload"] == workload) &
                            (df["bs"] == bs) &
                            (df["numjobs"] == nj)
                        ]
                        val = subdf[r_name].iloc[0] if not subdf.empty else None
                        c = ws.cell(current_row, i, val)
                        c.alignment = center; c.border = border
                    current_row += 1

                current_row += 3
                written_tables += 1

    ws.column_dimensions["A"].width = 14
    for col in ["B","C","D","E"]:
        ws.column_dimensions[col].width = 12

    out = Path(output_path) if output_path else base / f"{base.name}_summary.xlsx"
    wb.save(out)
    print(f"[OK] Created {written_tables} tables -> {out}")

if __name__ == "__main__":
    log_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    output_path = sys.argv[2] if len(sys.argv) > 2 else None
    main(log_dir, output_path)

