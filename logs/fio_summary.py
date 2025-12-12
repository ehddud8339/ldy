#!/usr/bin/env python3
import os
import json
import glob
import argparse
import pandas as pd


DEFAULT_INPUT = "./"
DEFAULT_OUTPUT = "fio_summary.xlsx"

# ================================
# 원하는 컬럼을 여기서 선택 (파일명 제외)
# 존재하지 않아도 무시됨
# ================================
SELECTED_COLUMNS = [
    "read_bw_MBps",
    "write_bw_MBps",
    "read_iops",
    "write_iops",
    "avg_latency_us",
    "p99_latency_us",
    "max_latency_us",
]
# ================================


def parse_filename(path: str):
    """
    filename 예시:
       rr_cpu-pinned_fio.log
       thr_rand_iops_fio.log
    → sched=rr, bound=cpu-pinned_fio (또는 나머지 전부)
    """
    base = os.path.basename(path)
    name, _ = os.path.splitext(base)

    parts = name.split("_", 1)
    sched = parts[0]
    bound = parts[1] if len(parts) > 1 else "default"

    return sched, bound


def load_fio_json(path: str):
    """
    fio 실행 시 stderr에 에러/경고가 섞여서
    한 파일에 "fio: ..." + JSON 이 같이 들어갈 수 있음.

    이 함수는:
      1) 파일 전체 문자열을 읽고
      2) 첫 번째 '{' 위치와 마지막 '}' 위치를 찾아
      3) 그 구간만 잘라서 json.loads() 시도
    """

    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        text = f.read()

    start = text.find("{")
    end = text.rfind("}")

    if start == -1 or end == -1 or end <= start:
        print(f"[WARN] Cannot find valid JSON in: {path}")
        return None

    json_str = text[start : end + 1]

    try:
        return json.loads(json_str)
    except json.JSONDecodeError as e:
        print(f"[WARN] JSON decode failed for {path}: {e}")
        return None


def parse_fio_json(path: str):
    """
    FIO JSON 파싱하여 metrics dict 반환
    (jobs[] 합산, latency는 weighted average)
    """
    data = load_fio_json(path)
    if not data:
        return {}

    jobs = data.get("jobs", [])
    if not jobs:
        return {}

    total_read_bw = 0.0
    total_write_bw = 0.0
    total_read_iops = 0.0
    total_write_iops = 0.0

    total_latency_ns = 0.0
    total_operations = 0
    max_latency_ns = 0.0
    p99_list = []

    for job in jobs:
        read = job.get("read", {}) or {}
        write = job.get("write", {}) or {}

        # BW bytes/sec → MB/s
        total_read_bw += read.get("bw_bytes", 0.0) / (1024 * 1024)
        total_write_bw += write.get("bw_bytes", 0.0) / (1024 * 1024)

        # IOPS
        total_read_iops += read.get("iops", 0.0)
        total_write_iops += write.get("iops", 0.0)

        # latency (lat_ns)
        lat_ns = job.get("lat_ns", {}) or {}
        mean_lat = lat_ns.get("mean", 0.0)
        ops = read.get("total_ios", 0) + write.get("total_ios", 0)

        total_latency_ns += mean_lat * ops
        total_operations += ops

        # max latency
        max_latency_ns = max(max_latency_ns, lat_ns.get("max", 0.0))

        # clat percentiles에서 99퍼센타일
        # read 쪽에 있으면 read 우선, 없으면 write에서 가져옴
        clat_read = job.get("read", {}).get("clat_ns", {}) or {}
        clat_write = job.get("write", {}).get("clat_ns", {}) or {}
        percentiles = clat_read.get("percentile") or clat_write.get("percentile") or {}

        if "99.000000" in percentiles:
            p99_list.append(percentiles["99.000000"])

    result = {}

    result["read_bw_MBps"] = round(total_read_bw, 3)
    result["write_bw_MBps"] = round(total_write_bw, 3)
    result["read_iops"] = round(total_read_iops, 2)
    result["write_iops"] = round(total_write_iops, 2)

    if total_operations > 0:
        avg_lat_ns = total_latency_ns / total_operations
        result["avg_latency_us"] = round(avg_lat_ns / 1000.0, 2)
    else:
        result["avg_latency_us"] = None

    if p99_list:
        p99_avg_ns = sum(p99_list) / len(p99_list)
        result["p99_latency_us"] = round(p99_avg_ns / 1000.0, 2)
    else:
        result["p99_latency_us"] = None

    result["max_latency_us"] = round(max_latency_ns / 1000.0, 2)

    return result


def main():
    parser = argparse.ArgumentParser(description="Parse FIO JSON logs (with noise) to XLSX")
    parser.add_argument(
        "-i", "--input",
        default=DEFAULT_INPUT,
        help="Input directory containing fio *.log/*.json (default: ./)",
    )
    parser.add_argument(
        "-o", "--output",
        default=DEFAULT_OUTPUT,
        help="Output XLSX filename (default: fio_summary.xlsx)",
    )

    args = parser.parse_args()

    log_files = glob.glob(os.path.join(args.input, "*.log")) + \
                glob.glob(os.path.join(args.input, "*.json"))

    rows = []

    for path in sorted(log_files):
        sched, bound = parse_filename(path)
        metrics = parse_fio_json(path)

        row = {
            "sched": sched,
            "bound": bound,
            "filename": os.path.basename(path),
        }
        row.update(metrics)
        rows.append(row)

    if not rows:
        print(f"⚠ No FIO logs found in: {args.input}")
        return

    df = pd.DataFrame(rows)

    # 메타 컬럼
    front = ["sched", "bound", "filename"]

    # 선택된 컬럼 중 실제 존재하는 것만
    metric_cols = [c for c in SELECTED_COLUMNS if c in df.columns]
    if not metric_cols:
        metric_cols = [c for c in df.columns if c not in front]

    df = df[front + metric_cols]

    # 저장
    os.makedirs(os.path.dirname(os.path.abspath(args.output)) or ".", exist_ok=True)

    with pd.ExcelWriter(args.output) as writer:
        for bound in sorted(df["bound"].unique()):
            subdf = df[df["bound"] == bound]
            sheet = bound[:31]
            subdf.to_excel(writer, sheet_name=sheet, index=False)

    print(f"📁 Saved → {args.output}")
    print(f"📌 Columns: {', '.join(front + metric_cols)}")


if __name__ == "__main__":
    main()

