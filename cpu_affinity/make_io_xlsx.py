#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
make_io_xlsx.py
- fio JSON ([section]_[bs]_[numjobs].json) → 엑셀 한 시트에 블록으로 정리
- 시트명: 출력 파일명(stem)
- 각 블록: 1행 병합 제목([section]_[bs]) / 2행 헤더(열=numjobs) / 3행~ 지표 행
- 퍼센타일은 clat_* / lat_* 모두 지원 + 95/99/99.5/99.9/99.95/99.99
- NaN/Inf는 빈 칸으로 기록(xlsxwriter가 NaN을 싫어하므로)
"""

import argparse
import json
import math
import re
from pathlib import Path
from typing import Dict, Any, Optional, Tuple, List

import pandas as pd

# 파일명 패턴: [section]_[bs]_[numjobs].json
FILENAME_RE = re.compile(r'^(?P<section>[A-Za-z0-9]+)_(?P<bs>[^_]+)_(?P<numjobs>\d+)\.json$')

# 추출할 퍼센타일 키들
PCTS = ["99.950000", "99.990000"]

def _norm_cpu_percent(v: Optional[float]) -> Optional[float]:
    if v is None:
        return None
    try:
        f = float(v)
    except Exception:
        return None
    # 0.12=12% 같은 분수 값이면 0–100으로 보정
    if 0.0 <= f <= 1.0:
        f *= 100.0
    return f

def _pick_percentiles(sec_dict: Dict[str, Any]) -> Dict[str, float]:
    """
    fio JSON에서 퍼센타일 표를 찾아 µs 단위로 반환.
    clat_ns/clat_us 또는 lat_ns/lat_us 어디에 있든 우선 발견되는 쪽 사용.
    return 예: {"99.950000": 1234.0, ...} (µs)
    """
    out: Dict[str, float] = {}
    for key in ("clat_ns", "clat_us", "lat_ns", "lat_us"):
        bucket = sec_dict.get(key, {})
        pct = bucket.get("percentile")
        if isinstance(pct, dict) and pct:
            for p in PCTS:
                v = pct.get(p)
                if v is not None:
                    out[p] = (float(v) / 1000.0) if key.endswith("_ns") else float(v)
            return out
    return out

def _agg_jobs(j: Dict[str, Any]) -> Dict[str, Any]:
    """
    jobs[*] 집계:
      - read/write: IOPS 합산, BW_bytes 합산(→ MB/s),
        평균 지연(µs) 가중평균(Σ(mean_us_i * ios_i)/Σios_i),
        p95/p99/p99.5/p99.9/p99.95/p99.99 (가능 시)
      - total: IOPS/BW 합
      - CPU%: 평균(필요 시 0–100 보정)
    """
    jobs = j.get("jobs", []) or []

    def agg_side(side: str):
        iops_sum = 0.0
        bwB_sum = 0.0
        ios_sum = 0
        mean_lat_num = 0.0  # Σ(mean_us_i * ios_i)
        # 퍼센타일 맵
        pmap: Dict[str, float] = {}

        # 그룹 퍼센타일(가능 시, jobs[0]에서 가져옴)
        if jobs:
            sec0 = jobs[0].get(side, {})
            pmap = _pick_percentiles(sec0)

        for x in jobs:
            s = x.get(side, {}) or {}
            iops_sum += float(s.get("iops", 0.0) or 0.0)
            bwB_sum += float(s.get("bw_bytes", 0.0) or 0.0)
            ios = int(s.get("total_ios", 0) or 0)
            ios_sum += ios

            mean_us = None
            if "lat_ns" in s and isinstance(s["lat_ns"], dict) and "mean" in s["lat_ns"]:
                mean_us = float(s["lat_ns"]["mean"]) / 1000.0
            elif "clat_ns" in s and isinstance(s["clat_ns"], dict) and "mean" in s["clat_ns"]:
                mean_us = float(s["clat_ns"]["mean"]) / 1000.0
            if mean_us is not None and ios > 0:
                mean_lat_num += mean_us * ios

        avg_us = (mean_lat_num / ios_sum) if ios_sum > 0 else math.nan
        MBps = bwB_sum / (1024.0 * 1024.0)
        return iops_sum, MBps, avg_us, ios_sum, pmap

    r_iops, r_bw, r_avg, r_ios, r_pcts = agg_side("read")
    w_iops, w_bw, w_avg, w_ios, w_pcts = agg_side("write")

    # CPU%
    if jobs:
        usr = sum(float(x.get("usr_cpu", 0.0) or 0.0) for x in jobs) / max(1, len(jobs))
        sys = sum(float(x.get("sys_cpu", 0.0) or 0.0) for x in jobs) / max(1, len(jobs))
    else:
        usr = sys = 0.0
    usr = _norm_cpu_percent(usr)
    sys = _norm_cpu_percent(sys)

    # 결과 dict
    out = {
        # total
        "IOPS_total": (r_iops + w_iops),
        "BW_total_MBps": (r_bw + w_bw),
        # read
        "avg_read_us": r_avg if r_ios > 0 else math.nan, 
        "p9995_read_us": r_pcts.get("99.950000"),
        "p9999_read_us": r_pcts.get("99.990000"),
        # write
        "avg_write_us": w_avg if w_ios > 0 else math.nan,
        "p9995_write_us": w_pcts.get("99.950000"),
        "p9999_write_us": w_pcts.get("99.990000"),
        # CPU
        "usr_cpu_pct": usr,
        "sys_cpu_pct": sys,
    }
    return out

def parse_filename(p: Path) -> Optional[Tuple[str, str, int]]:
    m = FILENAME_RE.match(p.name)
    if not m:
        return None
    return m.group("section"), m.group("bs"), int(m.group("numjobs"))

def load_rows(input_dir: Path) -> pd.DataFrame:
    rows: List[Dict[str, Any]] = []
    for p in sorted(input_dir.glob("*.json")):
        meta = parse_filename(p)
        if not meta:
            continue
        section, bs, numjobs = meta
        try:
            with p.open("r", encoding="utf-8") as f:
                j = json.load(f)
        except Exception as e:
            print(f"[warn] JSON parse failed: {p.name}: {e}")
            continue
        agg = _agg_jobs(j)
        rows.append({
            "section": section,
            "bs": bs,
            "numjobs": numjobs,
            **agg,
        })
    return pd.DataFrame(rows)

def build_block_matrix(grp: pd.DataFrame) -> Tuple[List[int], pd.DataFrame]:
    """
    (section, bs) 그룹 → 열=numjobs, 행=지표 행렬 생성
    """
    grp_sorted = grp.sort_values("numjobs")
    numjobs_list = grp_sorted["numjobs"].tolist()

    metrics = [
        "IOPS_total",
        "BW_total_MBps",
        "avg_read_us", "p95_read_us", "p99_read_us", "p999_read_us", "p9990_read_us", "p9995_read_us", "p9999_read_us",
        "avg_write_us", "p95_write_us", "p99_write_us", "p999_write_us", "p9990_write_us", "p9995_write_us", "p9999_write_us",
        "usr_cpu_pct", "sys_cpu_pct",
    ]

    data: Dict[int, List[float]] = {}
    for nj, row in zip(numjobs_list, grp_sorted.to_dict("records")):
        col_vals = [row.get(m, math.nan) for m in metrics]
        data[nj] = col_vals

    mat = pd.DataFrame(data, index=metrics)
    return numjobs_list, mat

def write_one_sheet(df: pd.DataFrame, out_xlsx: Path, sheet_name: str):
    # 시트 이름 길이 제한 대응
    if len(sheet_name) > 31:
        sheet_name = sheet_name[:31]

    with pd.ExcelWriter(out_xlsx, engine="xlsxwriter") as writer:
        workbook  = writer.book
        worksheet = workbook.add_worksheet(sheet_name)
        writer.sheets[sheet_name] = worksheet

        # 포맷
        title_fmt = workbook.add_format({
            "bold": True, "align": "center", "valign": "vcenter",
            "border": 1, "bg_color": "#F2F2F2"
        })
        header_fmt = workbook.add_format({
            "bold": True, "align": "center", "valign": "vcenter",
            "border": 1
        })
        idx_fmt = workbook.add_format({
            "bold": True, "align": "left", "valign": "vcenter",
            "border": 1
        })
        cell_fmt = workbook.add_format({"border": 1})
        num_fmt  = workbook.add_format({"border": 1, "num_format": "0.00"})
        int_fmt  = workbook.add_format({"border": 1, "num_format": "0"})

        # 열 폭
        worksheet.set_column(0, 0, 22)  # 지표명
        worksheet.set_column(1, 100, 12)

        start_row = 0
        # (section, bs) 블록 쌓기
        for (section, bs), grp in df.groupby(["section", "bs"], sort=True):
            label = f"{section}_{bs}"
            numjobs_list, mat = build_block_matrix(grp)

            # 1) 병합 제목
            end_col = 1 + len(numjobs_list)  # 0: metric, 1..N: numjobs
            worksheet.merge_range(start_row, 0, start_row, end_col, label, title_fmt)

            # 2) 헤더 행
            worksheet.write(start_row + 1, 0, "", header_fmt)
            for idx, nj in enumerate(numjobs_list, start=1):
                worksheet.write(start_row + 1, idx, nj, header_fmt)

            # 3) 데이터 행
            for ridx, metric in enumerate(mat.index.tolist(), start=0):
                row = start_row + 2 + ridx
                worksheet.write(row, 0, metric, idx_fmt)
                for cidx, nj in enumerate(numjobs_list, start=1):
                    val = mat.loc[metric, nj]
                    # 서식 선택
                    if metric == "IOPS_total":
                        fmt = int_fmt
                    elif metric.startswith("IOPS") or metric.endswith("_pct"):
                        fmt = num_fmt
                    elif metric.endswith("_MBps"):
                        fmt = num_fmt
                    elif metric.endswith("_us"):
                        fmt = num_fmt
                    else:
                        fmt = cell_fmt
                    # NaN/Inf → 빈칸, 유한수만 숫자로 기록
                    if isinstance(val, (int, float)) and math.isfinite(val):
                        worksheet.write(row, cidx, val, fmt)
                    else:
                        worksheet.write_blank(row, cidx, None, fmt)

            # 블록 끝: 공백 1행
            start_row = start_row + 2 + len(mat.index) + 1

    print(f"[ok] wrote: {out_xlsx} (sheet: {sheet_name})")

def main():
    ap = argparse.ArgumentParser(description="fio JSON → single-sheet XLSX (blocks per [section]_[bs], columns=numjobs)")
    ap.add_argument("-i", "--input-dir", type=str, default="fio_logs",
                    help="Directory containing fio JSON files named [section]_[bs]_[numjobs].json")
    ap.add_argument("-o", "--output", type=str, default="fio_results.xlsx",
                    help="Output xlsx path; sheet name = file stem (e.g., report.xlsx → 'report')")
    args = ap.parse_args()

    input_dir = Path(args.input_dir).resolve()
    out_xlsx  = Path(args.output).resolve()
    sheet_name = out_xlsx.stem

    df = load_rows(input_dir)
    if df.empty:
        print("[info] no JSON files found (pattern: [section]_[bs]_[numjobs].json)")
        return

    # 정렬
    df.sort_values(by=["section", "bs", "numjobs"], inplace=True, ignore_index=True)
    write_one_sheet(df, out_xlsx, sheet_name)

if __name__ == "__main__":
    main()

