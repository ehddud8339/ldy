#!/usr/bin/env python3
import os, re, argparse
import pandas as pd
from collections import defaultdict
from openpyxl import Workbook
from openpyxl.styles import Alignment, Font
from openpyxl.utils import get_column_letter

# ----------------- CLI -----------------
parser = argparse.ArgumentParser(description="FIO logs → Excel (single-sheet, busy/idle side-by-side blocks like format.xlsx)")
parser.add_argument("--log_dir", type=str, default="./", help="로그 파일 디렉토리")
parser.add_argument("--output", type=str, default="summary_formatted.xlsx", help="출력 xlsx 파일명")
args = parser.parse_args()

LOG_DIR = args.log_dir
OUTPUT_FILE = args.output

# ----------------- Settings -----------------
# 파일명 패턴: [cpus]_[workload]_[bs]_[numjobs].log
LOG_PATTERN = re.compile(r"([\d,]+)_([a-z]+)_(\d+k)_(\d+)\.log$", re.IGNORECASE)
BUSY_CPUS_STR = "0,2,4,6"
IDLE_CPUS_STR = "8,10,12,14"
BUSY_CPUS = set(map(int, BUSY_CPUS_STR.split(",")))
IDLE_CPUS = set(map(int, IDLE_CPUS_STR.split(",")))

WORKLOADS_ORDER = ["read", "write", "randread", "randwrite"]
BS_ORDER = ["4k", "128k"]
METRICS = ["IOPS", "BW(MiB/s)", "lat_avg(ns)", "lat_p95(ns)", "lat_p99(ns)"]
NUMJOBS_ORDER = [1, 2, 3, 4]

# 각 (workload, bs) 그룹 사이에 넣을 공백 열 개수
GROUP_GAP = 2
# busy와 idle 블록 사이 공백 열(요청 없으면 0으로)
BUSY_IDLE_GAP = 0

# ----------------- FIO log parser -----------------
def parse_fio_log(filepath):
    """
    fio 텍스트 로그(표준 human-readable)에서 필요한 메트릭 추출
    반환 단위:
      IOPS: 정수
      BW(MiB/s): float (MiB/s로 통일)
      lat_*: float (ns 단위, fio 출력 단위가 ns라고 가정)
    """
    out = {
        "IOPS": None,
        "BW(MiB/s)": None,
        "lat_avg(ns)": None,
        "lat_p95(ns)": None,
        "lat_p99(ns)": None,
    }
    with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            # IOPS, BW 라인
            if "IOPS=" in line and "BW=" in line:
                m_iops = re.search(r"IOPS=(\d+(?:\.\d+)?[kK]?)", line)
                m_bw = re.search(r"BW=(\d+(?:\.\d+)?)(MiB/s|KiB/s|MB/s)", line, re.IGNORECASE)
                if m_iops:
                    v = m_iops.group(1)
                    if v.lower().endswith("k"):
                        out["IOPS"] = int(float(v[:-1]) * 1000.0)
                    else:
                        out["IOPS"] = int(float(v))
                if m_bw:
                    bw = float(m_bw.group(1))
                    unit = m_bw.group(2).lower()
                    # 통일: MiB/s
                    if unit.startswith("kib"):
                        bw = bw / 1024.0
                    elif unit == "mb/s":
                        # MB/s → MiB/s 근사 변환
                        bw = bw * 0.953674
                    out["BW(MiB/s)"] = round(bw, 2)

            # 평균 latency 라인
            if "avg=" in line and "stdev" in line:
                m = re.search(r"avg=([\d\.]+)", line)
                if m:
                    out["lat_avg(ns)"] = float(m.group(1))

            # 퍼센타일 라인
            if "95.00th=[" in line or "99.00th=[" in line:
                p95 = re.search(r"95\.00th=\[([\d\.]+)\]", line)
                p99 = re.search(r"99\.00th=\[([\d\.]+)\]", line)
                if p95:
                    out["lat_p95(ns)"] = float(p95.group(1))
                if p99:
                    out["lat_p99(ns)"] = float(p99.group(1))
    return out

def classify_by_cpu_string(cpu_str: str):
    """
    파일명 앞의 [cpus] 문자열로 busy/idle 판정.
    지정된 집합과 정확히 같으면 해당 그룹으로 간주.
    """
    try:
        s = set(map(int, cpu_str.split(",")))
    except Exception:
        return "other"
    if s == BUSY_CPUS:
        return "busy"
    if s == IDLE_CPUS:
        return "idle"
    # (혹시 부분집합이면 필요에 따라 로직 조정 가능)
    return "other"

# ----------------- Load logs -----------------
# 데이터 구조: data[(group, workload, bs, numjobs)] = {metric:value, ...}
data = {}
for fn in os.listdir(LOG_DIR):
    if not fn.endswith(".log"):
        continue
    m = LOG_PATTERN.match(fn)
    if not m:
        continue
    cpu_str, wl, bs, nj = m.groups()
    group = classify_by_cpu_string(cpu_str)
    if group not in ("busy", "idle"):
        continue
    try:
        nj = int(nj)
    except Exception:
        continue
    metrics = parse_fio_log(os.path.join(LOG_DIR, fn))
    data[(group, wl, bs, nj)] = metrics

if not data:
    raise SystemExit(f"No matching .log files in {LOG_DIR} (expecting names like '[cpus]_[workload]_[bs]_[numjobs].log')")

# ----------------- Excel layout helpers -----------------
def write_block(ws, start_col: int, header_text: str, block_values: dict):
    """
    start_col 열부터 4개(numjobs=1..4) 열을 사용해 블록을 그린다.
    행 구조(상단에서부터):
      R1: 머지 헤더  (header_text)  ← start_col..start_col+3 병합
      R2: numjobs 헤더(1,2,3,4)
      R3..R7: METRICS 순서대로 값
    block_values: dict[numjobs] -> dict(metric->value)
    """
    # 상단 머지 헤더
    ws.merge_cells(start_row=1, start_column=start_col, end_row=1, end_column=start_col + 3)
    hdr_cell = ws.cell(row=1, column=start_col, value=header_text)
    hdr_cell.alignment = Alignment(horizontal="center", vertical="center")
    hdr_cell.font = Font(bold=True)

    # numjobs 헤더
    for i, nj in enumerate(NUMJOBS_ORDER):
        ws.cell(row=2, column=start_col + i, value=str(nj)).alignment = Alignment(horizontal="center")

    # 메트릭 레이블(왼쪽 고정, 블록 옆에 이미 한 번만 써도 되지만 여기선 깔끔히 각 블록 아래 행에 값만 씀)
    # 값 채우기: METRICS × numjobs
    for r, metric in enumerate(METRICS, start=3):
        for i, nj in enumerate(NUMJOBS_ORDER):
            val = block_values.get(nj, {}).get(metric, None)
            ws.cell(row=r, column=start_col + i, value=val)

def collect_block_values(group: str, workload: str, bs: str):
    """
    특정 (group, workload, bs)에 대해
    { numjobs: {metric: value} } dict 생성
    """
    block = {}
    for nj in NUMJOBS_ORDER:
        m = data.get((group, workload, bs, nj), {})
        block[nj] = {metric: m.get(metric) for metric in METRICS}
    return block

# ----------------- Build sheet -----------------
wb = Workbook()
ws = wb.active
ws.title = "summary"

# 왼쪽에 메트릭 레이블 컬럼을 한 번만 박아두자 (3~7행)
ws.cell(row=2, column=1, value="numjobs →")
for idx, name in enumerate(METRICS, start=3):
    ws.cell(row=idx, column=1, value=name).font = Font(bold=True)

current_col = 2  # 실제 블록 시작 열(메트릭 레이블 다음 열에서 시작)

for wl in WORKLOADS_ORDER:
    for bs in BS_ORDER:
        # busy 블록
        busy_header = f"{BUSY_CPUS_STR}_{wl}_{bs}"
        busy_vals = collect_block_values("busy", wl, bs)
        write_block(ws, current_col, busy_header, busy_vals)

        # idle 블록 (busy 바로 오른쪽)
        idle_header = f"{IDLE_CPUS_STR}_{wl}_{bs}"
        idle_vals = collect_block_values("idle", wl, bs)
        idle_start = current_col + 4 + BUSY_IDLE_GAP
        write_block(ws, idle_start, idle_header, idle_vals)

        # 다음 그룹으로 이동
        current_col = idle_start + 4 + GROUP_GAP

# 보기 좋게 열 너비 자동(간단 추정)
for col in range(1, current_col):
    ws.column_dimensions[get_column_letter(col)].width = 12

# 최상단 왼쪽 코멘트
ws.cell(row=1, column=1, value="metric").font = Font(bold=True)

wb.save(OUTPUT_FILE)
print(f"✅ Excel 생성 완료: {OUTPUT_FILE}")

