#!/usr/bin/env python3
# -*- coding: utf-8 -*-

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
    # fio가 0.12(=12%)처럼 분수로 줄 수 있으므로 0–100%로 보정
    if 0.0 <= f <= 1.0:
        f *= 100.0
    return f

def _agg_jobs(j: Dict[str, Any]) -> Dict[str, Any]:
    """
    jobs[*]를 집계:
      - read/write 각각: IOPS 합산, BW_bytes 합산(→ MB/s), 평균 지연(µs) 가중평균,
        p95/p99_us(그룹 퍼센타일 있으면 사용).
      - CPU%: 평균(필요 시 0–100 보정)
    추가로 total 지표(IOPS/BW) 계산.
    """
    jobs = j.get("jobs", []) or []

    def agg_side(side: str):
        iops_sum = 0.0
        bwB_sum = 0.0
        ios_sum = 0
        mean_lat_num = 0.0  # Σ(mean_us_i * ios_i)
        p95_us = None
        p99_us = None

        # 그룹 퍼센타일(가능 시)
        if jobs:
            sec0 = jobs[0].get(side, {})
            for key in ("clat_ns", "clat_us"):
                pct = sec0.get(key, {}).get("percentile")
                if isinstance(pct, dict):
                    p95 = pct.get("95.000000")
                    p99 = pct.get("99.000000")
                    if p95 is not None:
                        p95_us = (p95 / 1000.0) if key.endswith("_ns") else float(p95)
                    if p99 is not None:
                        p99_us = (p99 / 1000.0) if key.endswith("_ns") else float(p99)
                    break

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
        # total(읽기+쓰기)
        "IOPS_total": (r_iops + w_iops),
        "BW_total_MBps": (r_bw + w_bw),

        # read
        "avg_read_us": r_avg if r_ios > 0 else math.nan,
        "p95_read_us": r_p95 if r_ios > 0 else math.nan,
        "p99_read_us": r_p99 if r_ios > 0 else math.nan,

        # write
        "avg_write_us": w_avg if w_ios > 0 else math.nan,
        "p95_write_us": w_p95 if w_ios > 0 else math.nan,
        "p99_write_us": w_p99 if w_ios > 0 else math.nan,

        # CPU
        "usr_cpu_pct": usr,
        "sys_cpu_pct": sys,
    }

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
    (section, bs) 그룹을 받아, 열= numjobs, 행= 주요 지표 형태의 표를 만든다.
    반환: (numjobs_list, matrix_df)
    """
    grp_sorted = grp.sort_values("numjobs")
    numjobs_list = grp_sorted["numjobs"].tolist()

    # 보여줄 지표(순서 고정)
    metrics = [
        "IOPS_total",
        "BW_total_MBps",
        "avg_read_us", "p95_read_us", "p99_read_us",
        "avg_write_us", "p95_write_us", "p99_write_us",
        "usr_cpu_pct", "sys_cpu_pct",
    ]

    # (rows=metrics, cols=numjobs)
    data = {}
    for nj, row in zip(numjobs_list, grp_sorted.to_dict("records")):
        col_vals = []
        for m in metrics:
            val = row.get(m, math.nan)
            col_vals.append(val)
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

        # 열 폭 대략 조정
        worksheet.set_column(0, 0, 20)  # 지표명
        worksheet.set_column(1, 100, 12)

        # (section, bs)로 그룹핑하여 블록을 위에서부터 쌓기
        start_row = 0
        for (section, bs), grp in df.groupby(["section", "bs"], sort=True):
            label = f"{section}_{bs}"
            numjobs_list, mat = build_block_matrix(grp)

            # 1) 상단 병합 제목: [section]_[bs]
            # 병합 범위: (start_row, 0) ~ (start_row, len(numjobs_list))  ← 왼쪽 지표명 칸 포함
            end_col = 1 + len(numjobs_list)  # 0:지표명, 1..N:numjobs
            worksheet.merge_range(start_row, 0, start_row, end_col, label, title_fmt)

            # 2) 헤더 행: (start_row+1)
            # A열(0): 공란(지표명 자리), B..: numjobs 값
            worksheet.write(start_row + 1, 0, "", header_fmt)
            for idx, nj in enumerate(numjobs_list, start=1):
                worksheet.write(start_row + 1, idx, nj, header_fmt)

            # 3) 데이터 행: 지표명 + 값들
            for ridx, metric in enumerate(mat.index.tolist(), start=0):
                row = start_row + 2 + ridx
                worksheet.write(row, 0, metric, idx_fmt)
                for cidx, nj in enumerate(numjobs_list, start=1):
                    val = mat.loc[metric, nj]

                    # 포맷 선택(기존 로직 그대로)
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

                    # ★ NaN/Inf 방지: 비유한수이면 blank로 기록
                    if isinstance(val, (int, float)) and math.isfinite(val):
                        worksheet.write(row, cidx, val, fmt)
                    else:
                        worksheet.write_blank(row, cidx, None, fmt)

            # 블록 끝난 뒤 빈 줄 하나
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
    sheet_name = out_xlsx.stem  # 요구사항 1: -o의 파일명(stem)을 시트명으로 사용

    df = load_rows(input_dir)
    if df.empty:
        print("[info] no JSON files found (pattern: [section]_[bs]_[numjobs].json)")
        return

    # (section, bs, numjobs) 정렬
    df.sort_values(by=["section", "bs", "numjobs"], inplace=True, ignore_index=True)

    write_one_sheet(df, out_xlsx, sheet_name)

if __name__ == "__main__":
    main()

