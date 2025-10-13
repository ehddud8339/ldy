import os, re, argparse
import pandas as pd
from collections import defaultdict
from openpyxl import Workbook
from openpyxl.utils.dataframe import dataframe_to_rows
from openpyxl.chart import BarChart, Reference

# ----------------- CLI -----------------
parser = argparse.ArgumentParser(description="FIO logs -> Excel (busy/idle pivots + charts)")
parser.add_argument("--log_dir", type=str, default="./", help="로그 파일 디렉토리")
parser.add_argument("--output", type=str, default="summary_with_graph.xlsx", help="출력 xlsx 파일명")
args = parser.parse_args()

LOG_DIR = args.log_dir
OUTPUT_FILE = args.output

# ----------------- Settings -----------------
LOG_PATTERN = re.compile(r"([\d,]+)_([a-z]+)_(\d+k)_(\d+)\.log")
BUSY_CPUS = {0, 2, 4, 6}
IDLE_CPUS = {8, 10, 12, 14}
METRICS_FOR_CHART = ["IOPS", "BW(MiB/s)", "lat_p95(ns)"]

# ----------------- Parsers -----------------
def parse_fio_log(filepath):
    out = {"IOPS": None, "BW(MiB/s)": None, "lat_avg(ns)": None, "lat_p95(ns)": None, "lat_p99(ns)": None}
    with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            # IOPS, BW
            if "IOPS=" in line and "BW=" in line:
                m_iops = re.search(r"IOPS=(\d+[kK]?)", line)
                m_bw = re.search(r"BW=(\d+)(MiB/s|KiB/s|MB/s|KiB/s)", line)
                if m_iops:
                    v = m_iops.group(1)
                    if v.lower().endswith("k"):
                        out["IOPS"] = int(float(v[:-1]) * 1000)
                    else:
                        out["IOPS"] = int(v)
                if m_bw:
                    bw = float(m_bw.group(1))
                    unit = m_bw.group(2).lower()
                    # 표준화: MiB/s 로 저장
                    if unit.startswith("kib"):
                        bw = bw / 1024.0
                    elif unit == "mb/s":
                        # MB→MiB 대략 변환(1 MB ≈ 0.9537 MiB)
                        bw = bw * 0.953674
                    out["BW(MiB/s)"] = round(bw, 2)

            # 평균 latency (ns)
            if "avg=" in line and "stdev" in line:
                m = re.search(r"avg=([\d\.]+)", line)
                if m:
                    out["lat_avg(ns)"] = float(m.group(1))

            # p95 / p99 (ns) — fio 포맷에서 대괄호 안 값
            if "95.00th=[" in line or "99.00th=[" in line:
                p95 = re.search(r"95\.00th=\[([\d\.]+)\]", line)
                p99 = re.search(r"99\.00th=\[([\d\.]+)\]", line)
                if p95:
                    out["lat_p95(ns)"] = float(p95.group(1))
                if p99:
                    out["lat_p99(ns)"] = float(p99.group(1))
    return out

def classify_cpu_group(cpu_str):
    cpus = {int(c) for c in cpu_str.split(",")}
    if cpus & BUSY_CPUS:
        return "busy"
    elif cpus & IDLE_CPUS:
        return "idle"
    else:
        return "other"

# ----------------- Load logs -----------------
rows = []
for fname in os.listdir(LOG_DIR):
    if not fname.endswith(".log"): 
        continue
    m = LOG_PATTERN.match(fname)
    if not m:
        continue
    cpu_str, workload, bs, numjobs = m.groups()
    d = parse_fio_log(os.path.join(LOG_DIR, fname))
    rows.append({
        "cpu_group": cpu_str,
        "type": classify_cpu_group(cpu_str),
        "workload": workload,
        "bs": bs,
        "numjobs": int(numjobs),
        **d,
    })

if not rows:
    raise SystemExit(f"No .log files parsed in {LOG_DIR}")

df = pd.DataFrame(rows)

# ----------------- Pivot (busy/idle) -----------------
def make_pivot(dfg):
    # columns: (metric, numjobs)
    pv = dfg.pivot_table(
        index=["workload", "bs"],
        columns="numjobs",
        values=["IOPS", "BW(MiB/s)", "lat_avg(ns)", "lat_p95(ns)", "lat_p99(ns)"],
        aggfunc="mean",
    ).sort_index()
    return pv

busy_pv = make_pivot(df[df["type"] == "busy"])
idle_pv = make_pivot(df[df["type"] == "idle"])

# ----------------- Flatten for writing & chart mapping -----------------
def flatten_for_excel(pv: pd.DataFrame):
    # MultiIndex columns: (metric, numjobs) -> "metric|nj<num>"
    flat_cols = []
    col_map_by_metric = defaultdict(list)  # metric -> list of column names
    for (metric, nj) in pv.columns:
        col_name = f"{metric}|nj{nj}"
        flat_cols.append(col_name)
        col_map_by_metric[metric].append(col_name)

    df_out = pv.copy()
    df_out.columns = flat_cols
    df_out = df_out.reset_index()  # bring 'workload','bs' as columns
    df_out.insert(0, "label", df_out["workload"] + "_" + df_out["bs"])
    return df_out, col_map_by_metric

busy_flat, busy_map = flatten_for_excel(busy_pv)
idle_flat, idle_map = flatten_for_excel(idle_pv)

# ----------------- Write to Excel -----------------
wb = Workbook()
ws_busy = wb.active
ws_busy.title = "busy"
ws_idle = wb.create_sheet("idle")

def write_df(ws, dframe: pd.DataFrame):
    for r in dataframe_to_rows(dframe, index=False, header=True):
        ws.append(r)

write_df(ws_busy, busy_flat)
write_df(ws_idle, idle_flat)

# ----------------- Charts -----------------
def add_metric_chart(ws, df_flat: pd.DataFrame, metric_map: dict, metric: str, anchor_cell: str):
    """Create a clustered bar chart for one metric across numjobs (series), categories = label rows."""
    # locate columns by header name
    headers = [ws.cell(row=1, column=c).value for c in range(1, ws.max_column + 1)]
    # 'label' column
    label_col = headers.index("label") + 1
    # numjobs columns for this metric
    metric_cols = []
    for colname in metric_map.get(metric, []):
        if colname in headers:
            metric_cols.append(headers.index(colname) + 1)
    if not metric_cols:
        return  # nothing to chart

    # categories = labels (rows 2..N)
    cats = Reference(ws, min_col=label_col, min_row=2, max_row=ws.max_row)

    chart = BarChart()
    chart.type = "col"
    chart.style = 10
    chart.title = f"{metric} by numjobs"
    chart.y_axis.title = metric
    chart.x_axis.title = "workload_bs"

    # add each numjobs series
    for col in metric_cols:
        data = Reference(ws, min_col=col, min_row=1, max_row=ws.max_row)  # include header
        chart.add_data(data, titles_from_data=True)

    chart.set_categories(cats)
    chart.width = 26
    chart.height = 12
    ws.add_chart(chart, anchor_cell)

# place charts on each sheet (three metrics)
add_metric_chart(ws_busy, busy_flat, busy_map, "IOPS", "L2")
add_metric_chart(ws_busy, busy_flat, busy_map, "BW(MiB/s)", "L20")
add_metric_chart(ws_busy, busy_flat, busy_map, "lat_p95(ns)", "L38")

add_metric_chart(ws_idle, idle_flat, idle_map, "IOPS", "L2")
add_metric_chart(ws_idle, idle_flat, idle_map, "BW(MiB/s)", "L20")
add_metric_chart(ws_idle, idle_flat, idle_map, "lat_p95(ns)", "L38")

wb.save(OUTPUT_FILE)
print(f"✅ Excel 생성 완료: {OUTPUT_FILE}")
print(f"  - busy rows: {len(busy_flat)}   idle rows: {len(idle_flat)}")
print(f"  - saved to: {OUTPUT_FILE}")

