#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_io_xlsx.py
- Reads fio JSON files named like: [mode]_[bs]_[numjobs].json
- Writes an Excel file with columns:
  Mode | Block Size | numjobs | IOPS (K) | BW (MB/s) | Avg Lat | p95 Lat | p99 Lat | sys_cpu % | usr_cpu %
  * Avg/p95/p99 Lat are in microseconds (µs)
Usage:
  python make_io_xlsx.py "*.json" -o io_results.xlsx
  python make_io_xlsx.py "/path/to/dir/*.json" -o out.xlsx
"""

import argparse
import glob
import json
import os
import re
from typing import Tuple, Optional, Dict, Any, List

import pandas as pd


def parse_filename(fname: str) -> Tuple[str, str, int]:
    """
    Parse [mode]_[bs]_[numjobs].json
    Returns (mode, bs, numjobs)
    """
    base = os.path.basename(fname)
    if not base.endswith(".json"):
        raise ValueError(f"Not a JSON file: {base}")
    stem = base[:-5]
    parts = stem.split("_")
    if len(parts) < 3:
        raise ValueError(f"Filename not in [mode]_[bs]_[numjobs].json format: {base}")
    mode = parts[0]
    bs = parts[1]
    try:
        numjobs = int(parts[2])
    except ValueError:
        # sometimes numjobs could include extra suffix; try extracting leading digits
        m = re.match(r"(\d+)", parts[2])
        if not m:
            raise
        numjobs = int(m.group(1))
    return mode, bs, numjobs


def to_mb_per_s(bw_bytes: float) -> float:
    # fio reports bw_bytes in bytes/sec
    return bw_bytes / (1024.0 * 1024.0)


def extract_lat_us(section: Dict[str, Any]) -> Tuple[Optional[float], Optional[float], Optional[float]]:
    """
    Get Avg / p95 / p99 latency in microseconds (µs)
    Prefer lat_ns.mean if present; fall back to clat_ns.mean. Percentiles from clat_ns.percentile.
    """
    avg_us = None
    p95_us = None
    p99_us = None

    # average
    if "lat_ns" in section and isinstance(section["lat_ns"].get("mean", None), (int, float)):
        avg_us = section["lat_ns"]["mean"] / 1_000.0
    elif "clat_ns" in section and isinstance(section["clat_ns"].get("mean", None), (int, float)):
        avg_us = section["clat_ns"]["mean"] / 1_000.0

    # percentiles
    pct = section.get("clat_ns", {}).get("percentile", {})
    # fio percentile keys are strings like "95.000000"
    if isinstance(pct, dict):
        p95 = pct.get("95.000000", None)
        p99 = pct.get("99.000000", None)
        if isinstance(p95, (int, float)):
            p95_us = p95 / 1_000.0
        if isinstance(p99, (int, float)):
            p99_us = p99 / 1_000.0

    return avg_us, p95_us, p99_us


def normalize_cpu_percent(x: Optional[float]) -> Optional[float]:
    """
    fio's usr_cpu/sys_cpu are usually percentages already (e.g., 17.3).
    But if a value is <= 1.0 we treat it as a fraction and convert to %.
    """
    if x is None:
        return None
    try:
        return float(x)
    except Exception:
        return None


def read_fio_json(path: str) -> Dict[str, Any]:
    with open(path, "r") as f:
        return json.load(f)


def pick_rw_section(job: Dict[str, Any]) -> Dict[str, Any]:
    """
    Return the non-empty section among job['read'] or job['write'].
    """
    r = job.get("read", {})
    w = job.get("write", {})
    # choose whichever has non-zero iops/bytes
    r_ios = r.get("total_ios", 0) or r.get("io_bytes", 0) or r.get("iops", 0)
    w_ios = w.get("total_ios", 0) or w.get("io_bytes", 0) or w.get("iops", 0)
    return r if r_ios else w


def collect_rows(files: List[str]) -> List[Dict[str, Any]]:
    rows = []
    for fp in files:
        try:
            mode, bs, numjobs = parse_filename(fp)
        except Exception as e:
            print(f"[skip] {fp}: {e}")
            continue

        try:
            data = read_fio_json(fp)
            job = data["jobs"][0]
            sec = pick_rw_section(job)

            iops = float(sec.get("iops", 0.0))
            bw_mb = to_mb_per_s(float(sec.get("bw_bytes", 0.0)))
            avg_us, p95_us, p99_us = extract_lat_us(sec)
            sys_cpu = normalize_cpu_percent(job.get("sys_cpu", None))
            usr_cpu = normalize_cpu_percent(job.get("usr_cpu", None))

            rows.append({
                "Mode": mode,
                "Block Size": bs,
                "numjobs": numjobs,
                "IOPS (K)": round(iops / 1000.0, 3) if iops is not None else None,
                "BW (MB/s)": round(bw_mb, 2),
                "Avg Lat": None if avg_us is None else round(avg_us, 2),
                "p95 Lat": None if p95_us is None else round(p95_us, 2),
                "p99 Lat": None if p99_us is None else round(p99_us, 2),
                "sys_cpu %": None if sys_cpu is None else round(sys_cpu, 2),
                "usr_cpu %": None if usr_cpu is None else round(usr_cpu, 2),
            })
        except Exception as e:
            print(f"[error] {fp}: {e}")
            # still emit a row with just filename-derived fields
            rows.append({
                "Mode": mode,
                "Block Size": bs,
                "numjobs": numjobs,
                "IOPS (K)": None,
                "BW (MB/s)": None,
                "Avg Lat": None,
                "p95 Lat": None,
                "p99 Lat": None,
                "sys_cpu %": None,
                "usr_cpu %": None,
            })
    return rows


def main():
    p = argparse.ArgumentParser(description="Convert fio JSONs ([mode]_[bs]_[numjobs].json) to Excel.")
    p.add_argument("glob_pattern", nargs="?", default="*.json",
                   help="Glob of input JSON files (e.g., 'rand*128k*.json' or '/path/*.json').")
    p.add_argument("-o", "--output", default="io_results.xlsx", help="Output xlsx path.")
    args = p.parse_args()

    files = sorted(glob.glob(args.glob_pattern))
    if not files:
        raise SystemExit(f"No files matched: {args.glob_pattern}")

    rows = collect_rows(files)
    df = pd.DataFrame(rows, columns=[
        "Mode", "Block Size", "numjobs",
        "IOPS (K)", "BW (MB/s)",
        "Avg Lat", "p95 Lat", "p99 Lat",
        "sys_cpu %", "usr_cpu %",
    ])

    # sort for readability
    df.sort_values(by=["Mode", "Block Size", "numjobs"], inplace=True)

    # write xlsx
    with pd.ExcelWriter(args.output, engine="xlsxwriter") as writer:
        df.to_excel(writer, index=False, sheet_name="results")

        # Optional: basic column widths
        ws = writer.sheets["results"]
        widths = [16, 12, 8, 10, 12, 10, 10, 10, 10, 10]
        for i, w in enumerate(widths):
            ws.set_column(i, i, w)

    print(f"Wrote: {args.output}  (rows={len(df)})")


if __name__ == "__main__":
    main()

