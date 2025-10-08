#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
merge_io_xlsx.py

입력:
  --ext4 ext4_results.xlsx
  --fuse fuse_results.xlsx
  --rfuse rfuse_results.xlsx

출력:
  -o merged_results.xlsx  (시트 = 각 (section, bs), 열 = numjobs, 행 = ext4/fuse/rfuse)
  각 시트는 메트릭 블록(IOPS, BW_MBps, lat_avg_us, p95_tail_lat_us, p99_tail_lat_us)으로 구성되며
  각 블록 상단에 병합 셀 제목을 둡니다.
"""

import argparse
import math
from pathlib import Path
from typing import Dict, Tuple, List, Optional

import pandas as pd
from openpyxl import load_workbook


# =========================
# 1) 블록 스택 포맷 파서
# =========================

def parse_blocks_from_sheet(xlsx_path: Path) -> Dict[Tuple[str, str], pd.DataFrame]:
    """
    우리가 만든 '블록 쌓기' 형식의 첫 시트를 파싱하여
    (section, bs) -> DataFrame(rows=metric names, cols=numjobs(int)) 형태로 반환
    """
    wb = load_workbook(filename=str(xlsx_path), data_only=True, read_only=True)
    ws = wb[wb.sheetnames[0]]

    blocks: Dict[Tuple[str, str], pd.DataFrame] = {}

    row_idx = 1
    maxrow = ws.max_row

    while row_idx <= maxrow:
        title_cell = ws.cell(row=row_idx, column=1).value
        if not title_cell:
            row_idx += 1
            continue

        title = str(title_cell).strip()
        if "_" not in title:
            row_idx += 1
            continue
        section, bs = title.split("_", 1)

        # header: numjobs (다음 행)
        header_row = row_idx + 1
        numjobs: List[int] = []
        col = 2
        while True:
            val = ws.cell(row=header_row, column=col).value
            if val is None:
                break
            try:
                numjobs.append(int(val))
            except Exception:
                break
            col += 1
        if not numjobs:
            row_idx += 1
            continue

        # data rows
        data_start = row_idx + 2
        r = data_start
        metrics: List[str] = []
        matrix: Dict[int, List[Optional[float]]] = {nj: [] for nj in numjobs}

        while r <= maxrow:
            metric_name = ws.cell(row=r, column=1).value
            if metric_name is None or str(metric_name).strip() == "":
                break
            metric_name = str(metric_name).strip()
            metrics.append(metric_name)

            for c_idx, nj in enumerate(numjobs, start=2):
                v = ws.cell(row=r, column=c_idx).value
                if isinstance(v, (int, float)):
                    matrix[nj].append(float(v) if math.isfinite(float(v)) else float("nan"))
                else:
                    matrix[nj].append(float("nan"))
            r += 1

        if metrics:
            df = pd.DataFrame({nj: matrix[nj] for nj in numjobs}, index=metrics)
            blocks[(section, bs)] = df

        # 다음 블록 (블록 뒤 한 줄 비움)
        row_idx = r + 1

    return blocks


def load_fs_book(path: Path) -> Dict[Tuple[str, str], pd.DataFrame]:
    return parse_blocks_from_sheet(path)


# =========================
# 2) 유틸: 섹션 타입/행 추출
# =========================

READ_SECTIONS = ("seqread", "randread", "read")
WRITE_SECTIONS = ("seqwrite", "randwrite", "write")

def is_read_section(section: str) -> bool:
    s = section.lower()
    return any(s.startswith(x) for x in READ_SECTIONS)

def pick_metric_row_names(section: str) -> Tuple[str, str, str]:
    """
    (lat_avg, p95, p99) 행 이름을 섹션 타입에 맞게 반환.
    """
    if is_read_section(section):
        return ("avg_read_us", "p95_read_us", "p99_read_us")
    else:
        return ("avg_write_us", "p95_write_us", "p99_write_us")


def extract_series_for_fs(section: str, blk: pd.DataFrame) -> Dict[str, pd.Series]:
    """
    블록 DF(rows=metric names, cols=numjobs)에서 필요한 행을 뽑아 반환.
    반환: {"IOPS":Series, "BW_MBps":Series, "lat_avg_us":Series, "p95_us":Series, "p99_us":Series}
    """
    lat_avg_name, p95_name, p99_name = pick_metric_row_names(section)

    def get_row(name: str) -> pd.Series:
        if name in blk.index:
            s = pd.to_numeric(blk.loc[name], errors="coerce")
            s.index = s.index.astype(int)
            return s
        return pd.Series([math.nan] * blk.shape[1], index=blk.columns.astype(int))

    return {
        "IOPS": get_row("IOPS_total"),
        "BW_MBps": get_row("BW_total_MBps"),
        "lat_avg_us": get_row(lat_avg_name),
        "p95_us": get_row(p95_name),
        "p99_us": get_row(p99_name),
    }


# =========================
# 3) 작성: per-(section,bs) 시트
# =========================

def write_section_sheet(writer: pd.ExcelWriter,
                        sheet_name: str,
                        section: str,
                        ext4_blk: Optional[pd.DataFrame],
                        fuse_blk: Optional[pd.DataFrame],
                        rfuse_blk: Optional[pd.DataFrame]) -> None:
    """
    한 (section, bs) 시트를 작성:
    - 열: numjobs (세 FS 합집합)
    - 행: ext4/fuse/rfuse
    - 블록: IOPS, BW_MBps, lat_avg_us, p95_tail_lat_us, p99_tail_lat_us (각 블록 상단 병합 제목)
    """
    wb = writer.book
    ws = wb.add_worksheet(sheet_name)
    writer.sheets[sheet_name] = ws

    header_fmt = wb.add_format({"bold": True, "align": "center", "valign": "vcenter", "border": 1})
    title_fmt  = wb.add_format({"bold": True, "align": "center", "valign": "vcenter", "border": 1, "bg_color": "#F2F2F2"})
    fs_fmt     = wb.add_format({"bold": True, "align": "left",   "valign": "vcenter", "border": 1})
    cell_fmt   = wb.add_format({"border": 1})
    num_fmt    = wb.add_format({"border": 1, "num_format": "0.00"})
    int_fmt    = wb.add_format({"border": 1, "num_format": "0"})

    ws.set_column(0, 0, 18)    # FS 라벨열
    ws.set_column(1, 200, 12)  # 데이터열

    # FS별 시리즈 추출
    fs_data = {}
    if ext4_blk is not None:
        fs_data["ext4"] = extract_series_for_fs(section, ext4_blk)
    if fuse_blk is not None:
        fs_data["fuse"] = extract_series_for_fs(section, fuse_blk)
    if rfuse_blk is not None:
        fs_data["rfuse"] = extract_series_for_fs(section, rfuse_blk)

    if not fs_data:
        return

    # numjobs 합집합
    union_nj = set()
    for g in fs_data.values():
        for s in g.values():
            union_nj.update(map(int, s.index.tolist()))
    nj_cols = sorted(union_nj)

    # 메트릭 블록 정의: (키, 제목표시)
    metric_blocks = [
        ("IOPS",           "IOPS"),
        ("BW_MBps",        "BW_MBps"),
        ("lat_avg_us",     "lat_avg_us"),
        ("p95_us",         "p95_tail_lat_us"),
        ("p99_us",         "p99_tail_lat_us"),
    ]

    start_row = 0
    for mkey, mtitle in metric_blocks:
        # 1) 병합 제목행 (첫 열 포함해서 0..len(nj_cols) 병합)
        ws.merge_range(start_row, 0, start_row, len(nj_cols), mtitle, title_fmt)

        # 2) 헤더행: "", numjobs...
        ws.write(start_row + 1, 0, "", header_fmt)
        for i, nj in enumerate(nj_cols, start=1):
            ws.write(start_row + 1, i, int(nj), header_fmt)

        # 3) ext4 / fuse / rfuse 행
        row = start_row + 2
        for fsname in ["ext4", "fuse", "rfuse"]:
            if fsname not in fs_data:
                # 해당 FS 없으면 빈 행
                ws.write(row, 0, fsname, fs_fmt)
                for i in range(1, len(nj_cols)+1):
                    ws.write_blank(row, i, None, cell_fmt)
                row += 1
                continue

            ws.write(row, 0, fsname, fs_fmt)
            s = fs_data[fsname][mkey]
            # reindex to union
            s = s.reindex(nj_cols)
            for c, nj in enumerate(nj_cols, start=1):
                v = s.loc[nj]
                if v is None or (isinstance(v, float) and not math.isfinite(v)):
                    ws.write_blank(row, c, None, cell_fmt)
                else:
                    fmt = int_fmt if mkey == "IOPS" else num_fmt
                    ws.write(row, c, float(v), fmt)
            row += 1

        # 4) 블록 간 빈 행
        start_row = row + 1


# =========================
# 4) 메인
# =========================

def main():
    ap = argparse.ArgumentParser(description="Merge ext4/fuse/rfuse XLSX into per-(section,bs) sheets with FS rows and numjobs columns.")
    ap.add_argument("--ext4", required=True, help="Path to ext4_results.xlsx")
    ap.add_argument("--fuse", required=True, help="Path to fuse_results.xlsx")
    ap.add_argument("--rfuse", required=True, help="Path to rfuse_results.xlsx")
    ap.add_argument("-o", "--output", default="merged_results.xlsx", help="Output XLSX")
    args = ap.parse_args()

    ext4_blocks = load_fs_book(Path(args.ext4))
    fuse_blocks = load_fs_book(Path(args.fuse))
    rfuse_blocks = load_fs_book(Path(args.rfuse))

    all_keys = set(ext4_blocks.keys()) | set(fuse_blocks.keys()) | set(rfuse_blocks.keys())
    if not all_keys:
        print("No blocks found in input workbooks.")
        return

    out_path = Path(args.output).resolve()
    with pd.ExcelWriter(out_path, engine="xlsxwriter") as writer:
        for (section, bs) in sorted(all_keys):
            sheet_name = f"{section}-{bs}"
            # 엑셀 시트명 제한
            if len(sheet_name) > 31:
                sheet_name = sheet_name[:31]

            write_section_sheet(
                writer,
                sheet_name,
                section,
                ext4_blocks.get((section, bs)),
                fuse_blocks.get((section, bs)),
                rfuse_blocks.get((section, bs)),
            )

    print(f"[ok] wrote: {out_path}")

if __name__ == "__main__":
    main()

