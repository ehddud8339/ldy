#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
fio JSON 결과(파일명: [section]_[bs]_[numjobs].json)를 읽어
엑셀로 내보냅니다. 시트=section-bs, 행=numjobs.
"""

import argparse
import json
import math
import re
from pathlib import Path
from typing import Dict, Any, Optional, Tuple, List

import pandas as pd


FILENAME_RE = re.compile(r'^(?P<section>[A-Za-z0-9]+)_(?P<bs>[^_]+)_(?P<numjobs>\d+)\.json$')

def _norm_cpu_percent(v: Optional[float]) -> Optional[float]:
    if v is None:
        return None
    try:
        f = float(v)
    except Exception:
        return None
    # fio가 0.12(=12%)처럼 분수로 줄 수 있으므로 보정
    if 0.0 <= f <= 1.0:
        f *= 100.0
    return f

def _agg_jobs(j: Dict[str, Any]) -> Dict[str, Any]:
    """
    jobs[*]를 집계하여 read/write 각각에 대해:
      - iops 합산
      - bw_bytes 합산 (-> MB/s로 변환)
      - total_ios 합산
      - 평균지연(µs) = (Σ(mean_us_i * ios_i)) / (Σ ios_i)
      - p95/p99_us: group_reporting 퍼센타일이 있으면 사용, 없으면 None
    또한 usr/sys CPU%는 평균을 취하고, 0~1이면 0~100으로 변환.
    """
    jobs = j.get("jobs", []) or []

    def agg_side(side: str) -> Tuple[float, float, float, int, Optional[float], Optional[float]]:
        iops_sum = 0.0
        bwB_sum = 0.0
        ios_sum = 0
        mean_lat_num = 0.0  # Σ(mean_us_i * ios_i)
        # p95/p99: group 퍼센타일(가능하면 사용)
        p95_us = None
        p99_us = None

        # 우선 그룹 퍼센타일 시도 (group_reporting=1 일 때 흔함)
        if jobs:
            sec0 = jobs[0].get(side, {})
            for key in ("clat_ns", "clat_us"):  # ns/µs 둘 다 대비
                pct = sec0.get(key, {}).get("percentile")
                if isinstance(pct, dict):
                    p95 = pct.get("95.000000")
                    p99 = pct.get("99.000000")
                    if p95 is not None:
                        p95_us = (p95 / 1000.0) if key.endswith("_ns") else float(p95)
                    if p99 is not None:
                        p99_us = (p99 / 1000.0) if key.endswith("_ns") else float(p99)
                    break  # 하나라도 찾으면 종료

        for x in jobs:
            s = x.get(side, {}) or {}
            iops_sum += float(s.get("iops", 0.0) or 0.0)
            bwB_sum += float(s.get("bw_bytes", 0.0) or 0.0)
            ios = int(s.get("total_ios", 0) or 0)
            ios_sum += ios

            # 평균지연: lat_ns.mean 또는 clat_ns.mean 사용(µs로 변환)
            mean_us = None
            if "lat_ns" in s and isinstance(s["lat_ns"], dict) and "mean" in s["lat_ns"]:
                mean_us = float(s["lat_ns"]["mean"]) / 1000.0
            elif "clat_ns" in s and isinstance(s["clat_ns"], dict) and "mean" in s["clat_ns"]:
                mean_us = float(s["clat_ns"]["mean"]) / 1000.0
            if mean_us is not None and ios > 0:
                mean_lat_num += mean_us * ios

        avg_us = (mean_lat_num / ios_sum) if ios_sum > 0 else math.nan
        MBps = bwB_sum / (1024.0 * 1024.0)
        return iops_sum, MBps, avg_us, ios_sum, p95_us, p99_us

    r_iops, r_bw, r_avg, r_ios, r_p95, r_p99 = agg_side("read")
    w_iops, w_bw, w_avg, w_ios, w_p95, w_p99 = agg_side("write")

    # CPU%
    if jobs:
        usr = sum(float(x.get("usr_cpu", 0.0) or 0.0) for x in jobs) / max(1, len(jobs))
        sys = sum(float(x.get("sys_cpu", 0.0) or 0.0) for x in jobs) / max(1, len(jobs))
    else:
        usr = sys = 0.0
    usr = _norm_cpu_percent(usr)
    sys = _norm_cpu_percent(sys)

    return {
        "IOPS_read": r_iops if r_ios > 0 else 0.0,
        "BW_read_MBps": r_bw if r_ios > 0 else 0.0,
        "avg_read_us": r_avg if r_ios > 0 else math.nan,
        "p95_read_us": r_p95 if r_ios > 0 else math.nan,
        "p99_read_us": r_p99 if r_ios > 0 else math.nan,

        "IOPS_write": w_iops if w_ios > 0 else 0.0,
        "BW_write_MBps": w_bw if w_ios > 0 else 0.0,
        "avg_write_us": w_avg if w_ios > 0 else math.nan,
        "p95_write_us": w_p95 if w_ios > 0 else math.nan,
        "p99_write_us": w_p99 if w_ios > 0 else math.nan,

        "usr_cpu_pct": usr,
        "sys_cpu_pct": sys,
    }

def parse_filename(p: Path) -> Optional[Tuple[str, str, int]]:
    m = FILENAME_RE.match(p.name)
    if not m:
        return None
    section = m.group("section")
    bs = m.group("bs")
    numjobs = int(m.group("numjobs"))
    return section, bs, numjobs

def collect_rows(input_dir: Path) -> pd.DataFrame:
    rows: List[Dict[str, Any]] = []
    for p in sorted(input_dir.glob("*.json")):
        meta = parse_filename(p)
        if not meta:
            # 패턴 불일치 파일은 무시
            continue
        section, bs, numjobs = meta
        try:
            with p.open("r", encoding="utf-8") as f:
                j = json.load(f)
        except Exception as e:
            print(f"[warn] JSON parse failed: {p.name}: {e}")
            continue

        agg = _agg_jobs(j)
        row = {
            "section": section,
            "bs": bs,
            "numjobs": numjobs,
            **agg,
            "_file": p.name,
        }
        rows.append(row)

    if not rows:
        return pd.DataFrame()  # 빈 DF

    df = pd.DataFrame(rows)
    # 정렬: section, bs, numjobs
    df.sort_values(by=["section", "bs", "numjobs"], inplace=True, ignore_index=True)
    return df

def write_excel(df: pd.DataFrame, out_path: Path) -> None:
    if df.empty:
        print("[info] no rows to write; nothing found.")
        return

    # 시트별로 분리 (section, bs)
    with pd.ExcelWriter(out_path, engine="xlsxwriter") as writer:
        for (section, bs), grp in df.groupby(["section", "bs"], sort=True):
            sheet_name = f"{section}-{bs}"
            # Excel 시트 이름 제한(<=31 chars)
            if len(sheet_name) > 31:
                sheet_name = sheet_name[:31]
            # 보여줄 컬럼 순서
            cols = [
                "numjobs",
                "IOPS_read", "BW_read_MBps", "avg_read_us", "p95_read_us", "p99_read_us",
                "IOPS_write", "BW_write_MBps", "avg_write_us", "p95_write_us", "p99_write_us",
                "usr_cpu_pct", "sys_cpu_pct",
                "_file",
            ]
            present = [c for c in cols if c in grp.columns]
            grp.sort_values("numjobs", inplace=True)
            grp[present].to_excel(writer, index=False, sheet_name=sheet_name)

        # 요약 시트(선택): 각 (section, bs)에서 peak IOPS/BW 등 하이라이트
        summary_rows = []
        for (section, bs), grp in df.groupby(["section", "bs"]):
            # 총 IOPS/BW는 read+write (워크로드가 편향되어 있으면 해당만 의미)
            grp = grp.copy()
            grp["IOPS_total"] = grp["IOPS_read"].fillna(0) + grp["IOPS_write"].fillna(0)
            grp["BW_total_MBps"] = grp["BW_read_MBps"].fillna(0) + grp["BW_write_MBps"].fillna(0)
            idx_iops = grp["IOPS_total"].idxmax()
            idx_bw = grp["BW_total_MBps"].idxmax()
            summary_rows.append({
                "section": section,
                "bs": bs,
                "peak_numjobs_IOPS": int(grp.loc[idx_iops, "numjobs"]),
                "peak_IOPS_total": float(grp.loc[idx_iops, "IOPS_total"]),
                "peak_numjobs_BW": int(grp.loc[idx_bw, "numjobs"]),
                "peak_BW_total_MBps": float(grp.loc[idx_bw, "BW_total_MBps"]),
            })
        pd.DataFrame(summary_rows).sort_values(["section", "bs"]).to_excel(
            writer, index=False, sheet_name="summary"
        )
    print(f"[ok] wrote: {out_path}")

def main():
    ap = argparse.ArgumentParser(description="Convert fio JSON results to XLSX (sheets: section-bs, rows: numjobs)")
    ap.add_argument("--input-dir", "-i", type=str, default="fio_logs",
                    help="Directory containing fio JSON files (pattern: [section]_[bs]_[numjobs].json)")
    ap.add_argument("--output", "-o", type=str, default="fio_results.xlsx",
                    help="Output XLSX file path")
    args = ap.parse_args()

    input_dir = Path(args.input_dir).resolve()
    out_path = Path(args.output).resolve()
    input_dir.mkdir(parents=True, exist_ok=True)

    df = collect_rows(input_dir)
    write_excel(df, out_path)

if __name__ == "__main__":
    main()

