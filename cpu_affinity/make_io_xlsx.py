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

PCTS = ["99.950000", "99.990000"]

def _norm_cpu_percent(v: Optional[float]) -> Optional[float]:
    if v is None:
        return None
    try:
        f = float(v)
    except Exception:
        return None
    if 0.0 <= f <= 1.0:
        f *= 100.0
    return f

def _pick_percentiles(sec_dict: Dict[str, Any]) -> Dict[str, float]:
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
    jobs = j.get("jobs", []) or []

    def agg_side(side: str):
        iops_sum = 0.0
        bwB_sum = 0.0
        ios_sum = 0
        mean_lat_num = 0.0
        pmap: Dict[str, float] = {}
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

    if jobs:
        usr = sum(float(x.get("usr_cpu", 0.0) or 0.0) for x in jobs) / max(1, len(jobs))
        sys = sum(float(x.get("sys_cpu", 0.0) or 0.0) for x in jobs) / max(1, len(jobs))
    else:
        usr = sys = 0.0
    usr = _norm_cpu_percent(usr)
    sys = _norm_cpu_percent(sys)

    return {
        "IOPS_total": (r_iops + w_iops),
        "BW_total_MBps": (r_bw + w_bw),
        # read 전용
        "avg_read_us": r_avg if r_ios > 0 else math.nan,
        "p9995_read_us": r_pcts.get("99.950000"),
        "p9999_read_us": r_pcts.get("99.990000"),
        # write 전용
        "avg_write_us": w_avg if w_ios > 0 else math.nan,
        "p9995_write_us": w_pcts.get("99.950000"),
        "p9999_write_us": w_pcts.get("99.990000"),
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
        except Exception:
            continue
        agg = _agg_jobs(j)
        rows.append({"section": section, "bs": bs, "numjobs": numjobs, **agg})
    return pd.DataFrame(rows)

def build_block_matrix(grp: pd.DataFrame, section: str) -> Tuple[List[int], pd.DataFrame]:
    grp_sorted = grp.sort_values("numjobs")
    numjobs_list = grp_sorted["numjobs"].tolist()

    if section.startswith("seqread") or section.startswith("randread") or section.startswith("read"):
        metrics = [
            "IOPS_total", "BW_total_MBps",
            "avg_read_us","p9995_read_us","p9999_read_us",
            "usr_cpu_pct","sys_cpu_pct"
        ]
    else:
        metrics = [
            "IOPS_total", "BW_total_MBps",
            "avg_write_us","p9995_write_us","p9999_write_us",
            "usr_cpu_pct","sys_cpu_pct"
        ]

    data: Dict[int, List[float]] = {}
    for nj, row in zip(numjobs_list, grp_sorted.to_dict("records")):
        col_vals = [row.get(m, math.nan) for m in metrics]
        data[nj] = col_vals

    mat = pd.DataFrame(data, index=metrics)
    return numjobs_list, mat

def write_one_sheet(df: pd.DataFrame, out_xlsx: Path, sheet_name: str):
    if len(sheet_name) > 31:
        sheet_name = sheet_name[:31]

    with pd.ExcelWriter(out_xlsx, engine="xlsxwriter") as writer:
        workbook  = writer.book
        worksheet = workbook.add_worksheet(sheet_name)
        writer.sheets[sheet_name] = worksheet

        title_fmt = workbook.add_format({"bold": True,"align":"center","valign":"vcenter","border":1,"bg_color":"#F2F2F2"})
        header_fmt= workbook.add_format({"bold": True,"align":"center","valign":"vcenter","border":1})
        idx_fmt   = workbook.add_format({"bold": True,"align":"left","valign":"vcenter","border":1})
        cell_fmt  = workbook.add_format({"border":1})
        num_fmt   = workbook.add_format({"border":1,"num_format":"0.00"})
        int_fmt   = workbook.add_format({"border":1,"num_format":"0"})

        worksheet.set_column(0,0,22)
        worksheet.set_column(1,100,12)

        start_row=0
        for (section, bs), grp in df.groupby(["section","bs"], sort=True):
            label=f"{section}_{bs}"
            numjobs_list, mat = build_block_matrix(grp, section)

            end_col=1+len(numjobs_list)
            worksheet.merge_range(start_row,0,start_row,end_col,label,title_fmt)

            worksheet.write(start_row+1,0,"",header_fmt)
            for idx,nj in enumerate(numjobs_list,start=1):
                worksheet.write(start_row+1,idx,nj,header_fmt)

            for ridx, metric in enumerate(mat.index.tolist(), start=0):
                row=start_row+2+ridx
                worksheet.write(row,0,metric,idx_fmt)
                for cidx,nj in enumerate(numjobs_list,start=1):
                    val=mat.loc[metric,nj]
                    if metric=="IOPS_total":
                        fmt=int_fmt
                    elif metric.endswith("_pct") or metric.endswith("_MBps") or metric.endswith("_us"):
                        fmt=num_fmt
                    else:
                        fmt=cell_fmt
                    if isinstance(val,(int,float)) and math.isfinite(val):
                        worksheet.write(row,cidx,val,fmt)
                    else:
                        worksheet.write_blank(row,cidx,None,fmt)

            start_row=start_row+2+len(mat.index)+1

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("-i","--input-dir",default="fio_logs")
    ap.add_argument("-o","--output",default="fio_results.xlsx")
    args=ap.parse_args()

    input_dir=Path(args.input_dir).resolve()
    out_xlsx=Path(args.output).resolve()
    sheet_name=out_xlsx.stem

    df=load_rows(input_dir)
    if df.empty:
        print("no data")
        return
    df.sort_values(by=["section","bs","numjobs"], inplace=True, ignore_index=True)
    write_one_sheet(df,out_xlsx,sheet_name)

if __name__=="__main__":
    main()

