
import re
import os
import argparse
from collections import defaultdict, OrderedDict
import pandas as pd

# --------- Regex helpers ---------
RE_FILENAME = re.compile(r'(?P<workload>[^_]+)_(?P<bs>[^_]+)_(?P<numjobs>\d+)\.log$', re.IGNORECASE)
RE_IOPS = re.compile(r'IOPS\s*=\s*([0-9]+(?:\.[0-9]+)?)\s*([kKmM]?)', re.IGNORECASE)
RE_BW = re.compile(r'BW\s*=\s*([0-9]+(?:\.[0-9]+)?)\s*([KMG]i?B|[KMG]B)/s', re.IGNORECASE)
RE_LAT_AVG = re.compile(r'lat\s*\((nsec|usec|msec|sec)\)\s*:\s*min=.*?avg=([0-9]+(?:\.[0-9]+)?)', re.IGNORECASE)
RE_PERCENTILES_HEADER = re.compile(r'(?:clat|lat)\s+percentiles\s*\((nsec|usec|msec|sec)\)\s*:', re.IGNORECASE)
RE_PLINE = re.compile(r'\s*(95\.00th|99\.00th)\s*=\s*\[\s*([0-9]+)\s*\]', re.IGNORECASE)
RE_CPU = re.compile(r'cpu\s*:\s*usr=([0-9]+(?:\.[0-9]+)?)%.*,?\s*sys=([0-9]+(?:\.[0-9]+)?)%', re.IGNORECASE)

UNIT_MULT = {
    "nsec": 1e-3,    # nsec -> usec
    "usec": 1.0,     # usec -> usec
    "msec": 1e3,     # msec -> usec
    "sec" : 1e6,     # sec  -> usec
}

def to_mb_per_s(val, unit):
    """Convert bandwidth value and textual unit to MB/s (decimal MB)."""
    unit = unit.lower()
    # Normalize to bytes per second first
    mult = 1.0
    if unit in ("kb", "kib"):
        mult = 1024.0 if unit == "kib" else 1000.0
    elif unit in ("mb", "mib"):
        mult = 1024.0**2 if unit == "mib" else 1000.0**2
    elif unit in ("gb", "gib"):
        mult = 1024.0**3 if unit == "gib" else 1000.0**3
    else:
        # fallback (unlikely): bytes
        mult = 1.0
    bps = float(val) * mult
    mbps = bps / (1000.0**2)  # MB/s in decimal (to match 'BW(mb/s)' in template)
    return mbps

def apply_suffix(num, suffix):
    """Apply k/M suffix used by fio for IOPS to produce absolute number."""
    if not suffix:
        return float(num)
    s = suffix.lower()
    if s == 'k':
        return float(num) * 1_000.0
    if s == 'm':
        return float(num) * 1_000_000.0
    return float(num)

def parse_percentiles(lines, start_idx, unit):
    """Parse percentile lines after the percentiles header; returns dict with 95, 99 keys in usec."""
    p95 = p99 = None
    for i in range(start_idx, min(start_idx + 30, len(lines))):
        m = RE_PLINE.search(lines[i])
        if m:
            key, val = m.group(1), int(m.group(2))
            us = val * UNIT_MULT[unit]
            if key.startswith("95"):
                p95 = us
            elif key.startswith("99"):
                p99 = us
        if p95 is not None and p99 is not None:
            break
    return p95, p99

def parse_fio_log(path):
    """Return dict with metrics from a single fio log file."""
    with open(path, 'r', errors='ignore') as f:
        text = f.read()
    lines = text.splitlines()

    # IOPS
    iops = None
    m = RE_IOPS.search(text)
    if m:
        iops = apply_suffix(m.group(1), m.group(2))

    # BW in MB/s
    bw = None
    m = RE_BW.search(text)
    if m:
        bw = to_mb_per_s(m.group(1), m.group(2))

    # avg latency in usec
    lat_avg = None
    m = RE_LAT_AVG.search(text)
    if m:
        unit, avg = m.group(1).lower(), float(m.group(2))
        lat_avg = avg * UNIT_MULT[unit]

    # percentiles
    p95 = p99 = None
    for idx, line in enumerate(lines):
        mh = RE_PERCENTILES_HEADER.search(line)
        if mh:
            unit = mh.group(1).lower()
            p95, p99 = parse_percentiles(lines, idx + 1, unit)
            if p95 is not None or p99 is not None:
                break

    # CPU usage
    usr_cpu = sys_cpu = None
    m = RE_CPU.search(text)
    if m:
        usr_cpu = float(m.group(1))
        sys_cpu = float(m.group(2))

    return {
        "IOPS": iops,
        "BW(mb/s)": bw,
        "lat_avg(us)": lat_avg,
        "p95_lat(us)": p95,
        "p99_lat(us)": p99,
        "usr_cpu(%)": usr_cpu,
        "sys_cpu(%)": sys_cpu,
    }

def build_blocks(log_dir):
    """Group parsed results into blocks keyed by (workload, bs)."""
    groups = defaultdict(dict)  # {(workload, bs): {numjobs: metrics_dict}}
    for fname in os.listdir(log_dir):
        m = RE_FILENAME.match(fname)
        if not m:
            continue
        workload = m.group("workload")
        bs = m.group("bs")
        numjobs = int(m.group("numjobs"))
        path = os.path.join(log_dir, fname)
        metrics = parse_fio_log(path)
        groups[(workload, bs)][numjobs] = metrics
    # sort inner dicts by numjobs
    ordered = OrderedDict()
    for key in sorted(groups.keys(), key=lambda x: (x[0], x[1])):
        inner = groups[key]
        ordered[key] = OrderedDict(sorted(inner.items(), key=lambda kv: kv[0]))
    return ordered

def write_excel(ordered_groups, out_path):
    """
    Write an Excel similar to 'format.xlsx':
    For each (workload, bs) block:
      Row 1: merged header "[workload]_[bs]"
      Row 2: numjobs headers (1,2,4,8,16,32,...)
      Next rows: IOPS, BW(mb/s), lat_avg(us), p95_lat(us), p99_lat(us)
    """
    metrics_order = ["IOPS", "BW(mb/s)", "lat_avg(us)", "p95_lat(us)", "p99_lat(us)"]
    with pd.ExcelWriter(out_path, engine="xlsxwriter") as writer:
        workbook = writer.book
        ws = workbook.add_worksheet("Sheet1")
        writer.sheets["Sheet1"] = ws

        fmt_header = workbook.add_format({"align": "center", "valign": "vcenter", "bold": True, "border":1})
        fmt_metric = workbook.add_format({"align": "left", "valign": "vcenter", "border":1})
        fmt_cell = workbook.add_format({"align": "right", "valign": "vcenter", "num_format": "0.00", "border":1})
        fmt_int = workbook.add_format({"align": "right", "valign": "vcenter", "num_format": "0", "border":1})

        row = 0
        for (workload, bs), series in ordered_groups.items():
            # Prepare columns: numjobs sorted
            numjobs_list = list(series.keys())
            ncols = len(numjobs_list)

            # Header merged cell
            title = f"{workload}_{bs}"
            ws.merge_range(row, 1, row, 1 + ncols - 1, title, fmt_header)
            row += 1

            # numjobs header row
            ws.write(row, 0, "", fmt_header)
            for j, nj in enumerate(numjobs_list, start=1):
                ws.write(row, j, nj, fmt_header)
            row += 1

            # Metrics rows
            for metric in metrics_order:
                ws.write(row, 0, metric, fmt_metric)
                for j, nj in enumerate(numjobs_list, start=1):
                    val = series[nj].get(metric)
                    if val is None:
                        ws.write(row, j, "", fmt_cell)
                    else:
                        # formatting: integers for IOPS, 2 decimals for others
                        if metric == "IOPS":
                            ws.write(row, j, round(val), fmt_int)
                        else:
                            ws.write(row, j, float(val), fmt_cell)
                row += 1

            # Blank spacer row between blocks
            row += 2

        # Set some column widths
        ws.set_column(0, 0, 16)  # metric name
        ws.set_column(1, 50, 12) # data columns

def main():
    ap = argparse.ArgumentParser(description="Summarize fio logs into Excel following format.xlsx layout.")
    ap.add_argument("log_dir", help="Directory containing [workload]_[bs]_[numjobs].log files")
    ap.add_argument("--out", help="Output xlsx path (default: [log_dir]_summary.xlsx)", default=None)
    args = ap.parse_args()

    log_dir = os.path.abspath(args.log_dir)
    if not os.path.isdir(log_dir):
        raise SystemExit(f"Not a directory: {log_dir}")

    ordered = build_blocks(log_dir)
    if not ordered:
        raise SystemExit(f"No matching logs found in {log_dir}")

    out_path = args.out or os.path.join(os.path.dirname(log_dir), f"{os.path.basename(log_dir)}_summary.xlsx")
    write_excel(ordered, out_path)
    print(f"Wrote summary: {out_path}")

if __name__ == "__main__":
    main()
