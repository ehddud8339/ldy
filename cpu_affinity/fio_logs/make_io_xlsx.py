#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_io_xlsx.py
- Reads fio JSON files named: [mode]_[bs]_[numjobs].json
- Writes an Excel with columns:
  Mode | Block Size | numjobs | IOPS (K) | BW (MB/s) | Avg Lat | p95 Lat | p99 Lat | sys_cpu % | usr_cpu %
  * All Lat columns are unified to microseconds (µs).
  * CPU % uses values as-is by default (no scaling).

Examples:
  python make_io_xlsx.py "./rfuse_base/*.json" -o rfuse_results.xlsx
  python make_io_xlsx.py "/path/*.json" -o out.xlsx --engine openpyxl
"""

import argparse
import glob
import json
import os
import re
from typing import Tuple, Optional, Dict, Any, List

import pandas as pd


# ----------------------------
# Parsing helpers
# ----------------------------

def parse_filename(fname: str) -> Tuple[str, str, int]:
    """Parse [mode]_[bs]_[numjobs].json → (mode, bs, numjobs)."""
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
        m = re.match(r"(\d+)", parts[2])
        if not m:
            raise
        numjobs = int(m.group(1))
    return mode, bs, numjobs


def to_mb_per_s(bw_bytes: float) -> float:
    """fio’s bw_bytes is bytes/sec → MB/s (MiB/s)."""
    return float(bw_bytes) / (1024.0 * 1024.0)


# ----------------------------
# Latency unification to µs
# ----------------------------

def _extract_mean_us_from(section: Dict[str, Any], key_base: str) -> Optional[float]:
    """
    Try mean from {key_base}_ns or {key_base}_us (fio has lat_ns, clat_ns).
    Returns microseconds (µs).
    """
    ns_key = f"{key_base}_ns"
    us_key = f"{key_base}_us"
    # ns preferred (most common)
    if ns_key in section and isinstance(section[ns_key].get("mean", None), (int, float)):
        return section[ns_key]["mean"] / 1_000.0
    # fallback: us (rare but possible)
    if us_key in section and isinstance(section[us_key].get("mean", None), (int, float)):
        return float(section[us_key]["mean"])
    # some fio dumps might have just "lat_ns": {"mean": ...} directly (no base prefix)
    if key_base == "lat" and "lat_ns" in section and isinstance(section["lat_ns"].get("mean", None), (int, float)):
        return section["lat_ns"]["mean"] / 1_000.0
    if key_base == "lat" and "lat_us" in section and isinstance(section["lat_us"].get("mean", None), (int, float)):
        return float(section["lat_us"]["mean"])
    return None


def _extract_pct_us_from(section: Dict[str, Any], key_base: str, percentile: str) -> Optional[float]:
    """
    Get percentile from {key_base}_ns.percentile dict (or *_us).
    Returns microseconds (µs).
    """
    ns_key = f"{key_base}_ns"
    us_key = f"{key_base}_us"
    if ns_key in section and "percentile" in section[ns_key]:
        val = section[ns_key]["percentile"].get(percentile)
        if isinstance(val, (int, float)):
            return val / 1_000.0
    if us_key in section and "percentile" in section[us_key]:
        val = section[us_key]["percentile"].get(percentile)
        if isinstance(val, (int, float)):
            return float(val)
    # clat_ns is the most common place for percentiles; also try direct access
    if "clat_ns" in section and "percentile" in section["clat_ns"]:
        val = section["clat_ns"]["percentile"].get(percentile)
        if isinstance(val, (int, float)):
            return val / 1_000.0
    if "clat_us" in section and "percentile" in section["clat_us"]:
        val = section["clat_us"]["percentile"].get(percentile)
        if isinstance(val, (int, float)):
            return float(val)
    return None


def extract_latency_us(section: Dict[str, Any]) -> Tuple[Optional[float], Optional[float], Optional[float]]:
    """
    Unified latency (µs):
      - Avg Lat: prefer lat_ns.mean, fallback clat_ns.mean
      - p95/p99: from clat_ns.percentile (95.000000, 99.000000) or *_us variants
    """
    # average: try lat.*, then clat.*
    avg_us = _extract_mean_us_from(section, "lat")
    if avg_us is None:
        avg_us = _extract_mean_us_from(section, "clat")

    p95_us = _extract_pct_us_from(section, "clat", "95.000000")
    p99_us = _extract_pct_us_from(section, "clat", "99.000000")
    return avg_us, p95_us, p99_us


# ----------------------------
# CPU percent handling
# ----------------------------

def normalize_cpu_percent(x: Optional[float], mode: str = "none") -> Optional[float]:
    """
    Normalize fio usr_cpu/sys_cpu to percentage.

    mode:
      - "none": use value as-is (recommended; fio already gives % on most builds).
      - "x100": multiply by 100 (use if your JSON stores cpu as fraction).
      - "auto": if 0.0 <= x <= 1.0 treat as fraction, else as %.
    """
    if x is None:
        return None
    try:
        x = float(x)
    except Exception:
        return None
    if mode == "none":
        return x
    if mode == "x100":
        return x * 100.0
    if mode == "auto":
        return x * 100.0 if 0.0 <= x <= 1.0 else x
    return x


# ----------------------------
# FIO I/O section pick
# ----------------------------

def pick_rw_section(job: Dict[str, Any]) -> Dict[str, Any]:
    """Return the non-empty section among job['read'] or job['write']."""
    r = job.get("read", {}) or {}
    w = job.get("write", {}) or {}
    # choose whichever has non-zero iops/bytes
    r_ios = r.get("total_ios", 0) or r.get("io_bytes", 0) or r.get("iops", 0)
    w_ios = w.get("total_ios", 0) or w.get("io_bytes", 0) or w.get("iops", 0)
    return r if r_ios else w


def read_fio_json(path: str) -> Dict[str, Any]:
    with open(path, "r") as f:
        return json.load(f)


def collect_rows(files: List[str], cpu_scale: str) -> List[Dict[str, Any]]:
    rows = []
    for fp in files:
        mode, bs, numjobs = parse_filename(fp)
        try:
            data = read_fio_json(fp)
            job = data["jobs"][0]
            sec = pick_rw_section(job)

            iops = float(sec.get("iops", 0.0))
            bw_mb = to_mb_per_s(float(sec.get("bw_bytes", 0.0)))
            avg_us, p95_us, p99_us = extract_latency_us(sec)

            sys_cpu = normalize_cpu_percent(job.get("sys_cpu", None), mode=cpu_scale)
            usr_cpu = normalize_cpu_percent(job.get("usr_cpu", None), mode=cpu_scale)

            rows.append({
                "Mode": mode,
                "Block Size": bs,
                "numjobs": int(numjobs),
                "IOPS (K)": round(iops / 1000.0, 3),
                "BW (MB/s)": round(bw_mb, 2),
                "Avg Lat": None if avg_us is None else round(avg_us, 2),
                "p95 Lat": None if p95_us is None else round(p95_us, 2),
                "p99 Lat": None if p99_us is None else round(p99_us, 2),
                "sys_cpu %": None if sys_cpu is None else round(sys_cpu, 2),
                "usr_cpu %": None if usr_cpu is None else round(usr_cpu, 2),
            })
        except Exception as e:
            # emit a row at least with filename-derived fields
            rows.append({
                "Mode": mode,
                "Block Size": bs,
                "numjobs": int(numjobs),
                "IOPS (K)": None,
                "BW (MB/s)": None,
                "Avg Lat": None,
                "p95 Lat": None,
                "p99 Lat": None,
                "sys_cpu %": None,
                "usr_cpu %": None,
            })
            print(f"[error] {fp}: {e}")
    return rows


# ----------------------------
# Main
# ----------------------------

def main():
    p = argparse.ArgumentParser(description="Convert fio JSONs ([mode]_[bs]_[numjobs].json) to Excel.")
    p.add_argument("glob_pattern", nargs="?", default="*.json",
                   help="Glob for input files, e.g. './rfuse_base/*.json'")
    p.add_argument("-o", "--output", default="io_results.xlsx", help="Output xlsx path.")
    p.add_argument("--engine", choices=["xlsxwriter", "openpyxl"], default="xlsxwriter",
                   help="Excel writer engine (install package accordingly).")
    p.add_argument("--cpu-scale", choices=["none", "x100", "auto"], default="none",
                   help="How to interpret usr_cpu/sys_cpu: none=as-is (default), x100=multiply by 100, auto=if <=1.0 then x100.")
    args = p.parse_args()

    files = sorted(glob.glob(args.glob_pattern))
    if not files:
        raise SystemExit(f"No files matched: {args.glob_pattern}")

    rows = collect_rows(files, cpu_scale=args.cpu_scale)
    df = pd.DataFrame(rows, columns=[
        "Mode", "Block Size", "numjobs",
        "IOPS (K)", "BW (MB/s)",
        "Avg Lat", "p95 Lat", "p99 Lat",
        "sys_cpu %", "usr_cpu %",
    ])
    df.sort_values(by=["Mode", "Block Size", "numjobs"], inplace=True)

    with pd.ExcelWriter(args.output, engine=args.engine) as writer:
        df.to_excel(writer, index=False, sheet_name="results")
        # widths
        ws = writer.sheets["results"]
        widths = [16, 12, 8, 10, 12, 10, 10, 10, 10, 10]
        for i, w in enumerate(widths):
            try:
                ws.set_column(i, i, w)  # works with xlsxwriter
            except Exception:
                pass  # openpyxl: ignore

    print(f"Wrote: {args.output} (rows={len(df)})")


if __name__ == "__main__":
    main()

