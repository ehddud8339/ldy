import os, re, argparse
import pandas as pd
from openpyxl import Workbook
from openpyxl.utils.dataframe import dataframe_to_rows

# ---------- CLI ----------
parser = argparse.ArgumentParser(description="FIO logs → Excel summary (busy vs idle side-by-side)")
parser.add_argument("--log_dir", type=str, default="./", help="로그 파일 디렉토리")
parser.add_argument("--output", type=str, default="summary_side.xlsx", help="출력 xlsx 파일명")
args = parser.parse_args()

LOG_DIR = args.log_dir
OUTPUT_FILE = args.output

# ---------- Settings ----------
LOG_PATTERN = re.compile(r"([\d,]+)_([a-z]+)_(\d+k)_(\d+)\.log")
BUSY_CPUS = {0, 2, 4, 6}
IDLE_CPUS = {8, 10, 12, 14}

# ---------- Parser ----------
def parse_fio_log(path):
    d = {"IOPS": None, "BW(MiB/s)": None, "lat_avg(ns)": None, "lat_p95(ns)": None, "lat_p99(ns)": None}
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            if "IOPS=" in line and "BW=" in line:
                m_iops = re.search(r"IOPS=(\d+[kK]?)", line)
                m_bw = re.search(r"BW=(\d+)(MiB/s|KiB/s|MB/s)", line)
                if m_iops:
                    v = m_iops.group(1)
                    d["IOPS"] = float(v[:-1]) * 1000 if v.lower().endswith("k") else float(v)
                if m_bw:
                    bw = float(m_bw.group(1))
                    u = m_bw.group(2).lower()
                    if "ki" in u: bw /= 1024
                    elif "mb" in u: bw *= 0.9537
                    d["BW(MiB/s)"] = round(bw, 2)
            if "avg=" in line and "stdev" in line:
                m = re.search(r"avg=([\d\.]+)", line)
                if m: d["lat_avg(ns)"] = float(m.group(1))
            if "95.00th=[" in line or "99.00th=[" in line:
                p95 = re.search(r"95\.00th=\[([\d\.]+)\]", line)
                p99 = re.search(r"99\.00th=\[([\d\.]+)\]", line)
                if p95: d["lat_p95(ns)"] = float(p95.group(1))
                if p99: d["lat_p99(ns)"] = float(p99.group(1))
    return d

def classify(cpu_str):
    cpus = {int(c) for c in cpu_str.split(",")}
    if cpus & BUSY_CPUS: return "busy"
    if cpus & IDLE_CPUS: return "idle"
    return "other"

# ---------- Load ----------
records = []
for fn in os.listdir(LOG_DIR):
    if not fn.endswith(".log"): continue
    m = LOG_PATTERN.match(fn)
    if not m: continue
    cpu, wl, bs, nj = m.groups()
    d = parse_fio_log(os.path.join(LOG_DIR, fn))
    records.append({
        "cpu": cpu, "type": classify(cpu), "workload": wl, "bs": bs, "numjobs": int(nj), **d
    })
df = pd.DataFrame(records)

if df.empty:
    raise SystemExit(f"No .log found in {LOG_DIR}")

# ---------- Pivot ----------
def make_pivot(df_type):
    return df_type.pivot_table(
        index=["workload", "bs"],
        columns="numjobs",
        values=["IOPS", "BW(MiB/s)", "lat_avg(ns)", "lat_p95(ns)", "lat_p99(ns)"]
    ).sort_index()

busy = make_pivot(df[df["type"] == "busy"])
idle = make_pivot(df[df["type"] == "idle"])

# ---------- Excel ----------
wb = Workbook()
ws = wb.active
ws.title = "summary"

# Convert DataFrame → list
busy_rows = list(dataframe_to_rows(busy, index=True, header=True))
idle_rows = list(dataframe_to_rows(idle, index=True, header=True))

# 공백 열 간격
GAP = 5

# busy 헤더 기록
for r_idx, row in enumerate(busy_rows, start=1):
    for c_idx, val in enumerate(row, start=1):
        ws.cell(row=r_idx, column=c_idx, value=val)

# idle 헤더 기록 (busy의 마지막 열 + GAP)
idle_col_offset = busy.shape[1] + 3 + GAP  # +3은 index 컬럼(workload, bs)
for r_idx, row in enumerate(idle_rows, start=1):
    for c_idx, val in enumerate(row, start=idle_col_offset):
        ws.cell(row=r_idx, column=c_idx, value=val)

wb.save(OUTPUT_FILE)
print(f"✅ Excel 저장 완료: {OUTPUT_FILE}")
print(f"busy cols: {busy.shape[1]} / idle cols: {idle.shape[1]}")

