#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import math
from pathlib import Path
from typing import Dict, Tuple, List, Any, Optional

import pandas as pd
from openpyxl import load_workbook

# ----------------------------
# Helpers: parse block-stacked XLSX (our previous format)
# ----------------------------

def parse_blocks_from_sheet(xlsx_path: Path) -> Dict[Tuple[str, str], pd.DataFrame]:
    """
    Read first (only) sheet of our stacked-block workbook and return:
    {
      (section, bs): DataFrame,   # rows = metric names, cols = numjobs(int), values=float|NaN
      ...
    }
    """
    wb = load_workbook(filename=str(xlsx_path), data_only=True, read_only=True)
    # 시트가 하나라고 가정(혹은 첫 시트 사용)
    ws = wb[wb.sheetnames[0]]

    blocks: Dict[Tuple[str, str], pd.DataFrame] = {}

    row_idx = 1  # 1-based in openpyxl
    maxcol = ws.max_column
    maxrow = ws.max_row

    while row_idx <= maxrow:
        title_cell = ws.cell(row=row_idx, column=1).value
        if not title_cell:
            row_idx += 1
            continue

        # title like: "seqread_4k"
        title = str(title_cell).strip()
        if "_" not in title:
            row_idx += 1
            continue
        section, bs = title.split("_", 1)

        # header row is next
        header_row = row_idx + 1
        # columns: from col=2.. until None
        numjobs: List[int] = []
        col = 2
        while True:
            val = ws.cell(row=header_row, column=col).value
            if val is None:
                break
            try:
                numjobs.append(int(val))
            except Exception:
                # non-integer header, stop
                break
            col += 1
        if not numjobs:
            # malformed block; skip it
            row_idx += 1
            continue

        # data rows start at row_idx + 2, until blank metric name appears
        data_start = row_idx + 2
        r = data_start
        metrics: List[str] = []
        matrix: Dict[int, List[Optional[float]]] = {nj: [] for nj in numjobs}

        while r <= maxrow:
            metric_name = ws.cell(row=r, column=1).value
            # stop at blank line or next block title (heuristic: contains '_' and next row has header)
            if metric_name is None or str(metric_name).strip() == "":
                break
            metric_name = str(metric_name).strip()
            # if the next line seems like a new block title, stop (rare case)
            if "_" in metric_name and len(metric_name.split("_", 1)[1]) > 0 and (ws.cell(row=r+1, column=1).value == ""):
                # be conservative; but usually we won't hit this branch
                break

            metrics.append(metric_name)
            # read row values across numjobs
            for c_idx, nj in enumerate(numjobs, start=2):
                v = ws.cell(row=r, column=c_idx).value
                # normalize to float or NaN
                if isinstance(v, (int, float)):
                    if math.isfinite(float(v)):
                        matrix[nj].append(float(v))
                    else:
                        matrix[nj].append(float("nan"))
                else:
                    matrix[nj].append(float("nan"))
            r += 1

        if metrics:
            df = pd.DataFrame({nj: matrix[nj] for nj in numjobs}, index=metrics)
            blocks[(section, bs)] = df

        # advance: skip data rows + one blank line (our writer left 1 blank row between blocks)
        row_idx = r + 1

    return blocks


def load_fs_book(path: Path) -> Dict[Tuple[str, str], pd.DataFrame]:
    """
    Load one FS result workbook (ext4/fuse/rfuse) and return blocks dict.
    """
    return parse_blocks_from_sheet(path)


# ----------------------------
# Merge triplet (ext4/fuse/rfuse) into per-(section,bs) sheets
# ----------------------------

READ_SECTIONS = ("seqread", "randread", "read")
WRITE_SECTIONS = ("seqwrite", "randwrite", "write")

def is_read_section(section: str) -> bool:
    s = section.lower()
    return any(s.startswith(x) for x in READ_SECTIONS)

def is_write_section(section: str) -> bool:
    s = section.lower()
    return any(s.startswith(x) for x in WRITE_SECTIONS)

def pick_metrics_for_section(section: str) -> Tuple[str, str, str]:
    """
    Return metric field names to pick from per-block DataFrame, depending on read/write section.
    (lat_avg, p95, p99) field names in our previous XLSX writer.
    """
    if is_read_section(section):
        return ("avg_read_us", "p95_read_us", "p99_read_us")
    else:
        return ("avg_write_us", "p95_write_us", "p99_write_us")

def extract_triplet_for_block(section: str, df: pd.DataFrame) -> Dict[str, pd.Series]:
    """
    From a block DF (rows=metric names, cols=numjobs), return:
    {
      "IOPS": Series(numjobs),
      "BW_MBps": Series(numjobs),
      "lat_avg_us": Series(numjobs),
      "p95_us": Series(numjobs),
      "p99_us": Series(numjobs)
    }
    """
    lat_avg_name, p95_name, p99_name = pick_metrics_for_section(section)

    def get_row(name: str) -> pd.Series:
        if name in df.index:
            return pd.to_numeric(df.loc[name], errors="coerce")
        # fallback: all NaN for these columns
        return pd.Series([math.nan] * df.shape[1], index=df.columns)

    out = {
        "IOPS": get_row("IOPS_total"),
        "BW_MBps": get_row("BW_total_MBps"),
        "lat_avg_us": get_row(lat_avg_name),
        "p95_us": get_row(p95_name),
        "p99_us": get_row(p99_name),
    }
    return out

def union_numjobs(*series_groups: List[Dict[str, pd.Series]]) -> List[int]:
    cols = set()
    for group in series_groups:
        for s in group.values():
            cols.update(map(int, s.index.tolist()))
    return sorted(cols)

def reindex_to_union(series: pd.Series, union_cols: List[int]) -> pd.Series:
    series.index = series.index.astype(int)
    s = series.reindex(union_cols)
    return s

def build_sheet_matrix(section: str,
                       ext4_blk: Optional[pd.DataFrame],
                       fuse_blk: Optional[pd.DataFrame],
                       rfuse_blk: Optional[pd.DataFrame]) -> Tuple[List[int], pd.DataFrame]:
    """
    Build final matrix rows for one (section, bs) across 3 FS.
    Rows order:
      ext4_IOPS, ext4_BW_MBps, ext4_lat_avg_us, ext4_p95_us, ext4_p99_us,
      (blank),
      fuse_IOPS, ...
      (blank),
      rfuse_IOPS, ...
    Columns: union of numjobs across available blocks.
    """
    fs_map = {}
    if ext4_blk is not None:
        fs_map["ext4"] = extract_triplet_for_block(section, ext4_blk)
    if fuse_blk is not None:
        fs_map["fuse"] = extract_triplet_for_block(section, fuse_blk)
    if rfuse_blk is not None:
        fs_map["rfuse"] = extract_triplet_for_block(section, rfuse_blk)

    if not fs_map:
        return [], pd.DataFrame()

    # union of numjobs
    union_cols = set()
    for g in fs_map.values():
        for s in g.values():
            union_cols.update(map(int, s.index.tolist()))
    union_cols = sorted(union_cols)

    # build rows
    row_labels: List[str] = []
    rows: List[List[Optional[float]]] = []

    def append_fs_block(fsname: str, group: Dict[str, pd.Series]):
        mapping_order = [("IOPS", "IOPS"),
                         ("BW_MBps", "BW_MBps"),
                         ("lat_avg_us", "lat_avg_us"),
                         ("p95_us", "p95_us"),
                         ("p99_us", "p99_us")]
        for key, label in mapping_order:
            row_labels.append(f"{fsname}_{label}")
            s = group[key]
            # reindex to union
            s2 = s.copy()
            s2.index = s2.index.astype(int)
            s2 = s2.reindex(union_cols)
            rows.append([ (float(v) if isinstance(v, (int,float)) and math.isfinite(v) else None) for v in s2.tolist() ])
        # blank separator
        row_labels.append("")
        rows.append([None] * len(union_cols))

    for fsname in ["ext4", "fuse", "rfuse"]:
        if fsname in fs_map:
            append_fs_block(fsname, fs_map[fsname])

    # drop trailing blank if last block appended one
    if row_labels and row_labels[-1] == "":
        row_labels.pop()
        rows.pop()

    mat = pd.DataFrame(rows, index=row_labels, columns=union_cols)
    return union_cols, mat


# ----------------------------
# Main
# ----------------------------

def main():
    ap = argparse.ArgumentParser(description="Merge ext4/fuse/rfuse XLSX into consolidated per-(section,bs) sheets.")
    ap.add_argument("--ext4", required=True, help="Path to ext4_results.xlsx")
    ap.add_argument("--fuse", required=True, help="Path to fuse_results.xlsx")
    ap.add_argument("--rfuse", required=True, help="Path to rfuse_results.xlsx")
    ap.add_argument("-o", "--output", default="merged_results.xlsx", help="Output XLSX")
    args = ap.parse_args()

    ext4_blocks = load_fs_book(Path(args.ext4))
    fuse_blocks = load_fs_book(Path(args.fuse))
    rfuse_blocks = load_fs_book(Path(args.rfuse))

    # gather all (section, bs) keys
    all_keys = set(ext4_blocks.keys()) | set(fuse_blocks.keys()) | set(rfuse_blocks.keys())
    if not all_keys:
        print("No blocks found in input workbooks.")
        return

    # Write per-(section,bs) sheets
    out_path = Path(args.output).resolve()
    with pd.ExcelWriter(out_path, engine="xlsxwriter") as writer:
        wb = writer.book
        for (section, bs) in sorted(all_keys):
            sheet_name = f"{section}-{bs}"
            if len(sheet_name) > 31:
                sheet_name = sheet_name[:31]

            ext4_blk = ext4_blocks.get((section, bs))
            fuse_blk = fuse_blocks.get((section, bs))
            rfuse_blk = rfuse_blocks.get((section, bs))

            cols, mat = build_sheet_matrix(section, ext4_blk, fuse_blk, rfuse_blk)
            if mat.empty:
                continue

            # Write with header row (blank + numjobs)
            ws = wb.add_worksheet(sheet_name)
            writer.sheets[sheet_name] = ws

            header_fmt = wb.add_format({"bold": True, "align": "center", "valign": "vcenter", "border": 1})
            idx_fmt    = wb.add_format({"bold": True, "align": "left",   "valign": "vcenter", "border": 1})
            num_fmt    = wb.add_format({"border": 1, "num_format": "0.00"})
            int_fmt    = wb.add_format({"border": 1, "num_format": "0"})
            cell_fmt   = wb.add_format({"border": 1})

            # column widths
            ws.set_column(0, 0, 22)
            ws.set_column(1, len(cols), 12)

            # header
            ws.write(0, 0, "", header_fmt)
            for i, nj in enumerate(cols, start=1):
                ws.write(0, i, int(nj), header_fmt)

            # body
            for r, label in enumerate(mat.index.tolist(), start=1):
                ws.write(r, 0, label, idx_fmt if label else cell_fmt)
                for c, nj in enumerate(cols, start=1):
                    v = mat.loc[label, nj]
                    if v is None or (isinstance(v, float) and not math.isfinite(v)):
                        ws.write_blank(r, c, None, cell_fmt)
                    else:
                        # choose int for IOPS rows
                        fmt = int_fmt if label.endswith("_IOPS") else num_fmt
                        ws.write(r, c, float(v), fmt)

    print(f"[ok] wrote: {out_path}")

if __name__ == "__main__":
    main()
