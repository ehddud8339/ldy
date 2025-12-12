#!/usr/bin/env python3
import os
import glob
import re
import argparse
import pandas as pd


DEFAULT_LOG_DIR = "filebench_20251114_"
DEFAULT_OUTPUT = "filebench_summary.xlsx"

# ================================
# 보고 싶은 컬럼 선택
# ================================
SELECTED_COLUMNS = [
    "IOSummary_ops",
    "IOSummary_ops_per_sec",
    "IOSummary_rd_ops_per_sec",
    "IOSummary_wr_ops_per_sec",
    "IOSummary_mb_per_sec",
    "IOSummary_ms_per_op",
    "IOSummary_cpu_us_per_op",
    "IOSummary_latency_ms",
]
# ================================


def parse_filename(path):
    """
    rr_webserver_filebench.log -> sched=rr, bound=webserver, phase=filebench

    YCSB용 스크립트와 동일한 규칙:
      <sched>_<bound...>_<phase>.log
    """
    base = os.path.basename(path)
    name, _ = os.path.splitext(base)
    parts = name.split("_")
    if len(parts) < 2:
        # 비표준 이름일 때 fallback
        return name, "", ""
    sched = parts[0]
    phase = parts[-1]
    bound = "_".join(parts[1:-1]) if len(parts) > 2 else ""
    return sched, bound, phase


def parse_filebench_log(path):
    """
    filebench 로그에서 마지막 IO Summary 라인을 파싱해서 dict로 반환.

    지원 형식 예시:
      61.544: IO Summary: 42723139 ops 711991.797 ops/s 237331/0 rd/wr 3708.3mb/s 0.0ms/op
      22815: 275.797: IO Summary: 39 ops, 3.900 ops/s, (1/0 r/w), 0.0mb/s, 76364us cpu/op, 5.7ms latency
    """
    metrics = {}

    # IO Summary 라인만 모아서 "마지막 것" 사용
    io_summary_lines = []
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            if "IO Summary:" in line:
                io_summary_lines.append(line.strip())

    if not io_summary_lines:
        return metrics

    line = io_summary_lines[-1]

    # ----- 패턴 1: 공백 위주 포맷 (1.4.9.x 계열에서 자주 보임) -----
    # 예: IO Summary: 42723139 ops 711991.797 ops/s 237331/0 rd/wr 3708.3mb/s 0.0ms/op
    m1 = re.search(
        r"IO Summary:\s+(\d+)\s+ops\s+([\d\.]+)\s+ops/s\s+(\d+)/(\d+)\s+rd/wr\s+"
        r"([\d\.]+)\s*mb/s\s+([\d\.]+)\s*ms/op",
        line,
        re.IGNORECASE,
    )

    # ----- 패턴 2: 콤마/괄호가 포함된 포맷 -----
    # 예: IO Summary: 39 ops, 3.900 ops/s, (1/0 r/w), 0.0mb/s, 76364us cpu/op, 5.7ms latency
    m2 = re.search(
        r"IO Summary:\s*(\d+)\s*ops,\s*([\d\.]+)\s*ops/s,\s*"
        r"\((\d+)/(\d+)\s*r/w\),\s*([\d\.]+)\s*mb/s,\s*"
        r"(\d+)\s*us\s*cpu/op,\s*([\d\.]+)\s*ms\s*(?:latency|/op)",
        line,
        re.IGNORECASE,
    )

    if m1:
        total_ops = int(m1.group(1))
        ops_per_sec = float(m1.group(2))
        rd_ops_per_sec = float(m1.group(3))
        wr_ops_per_sec = float(m1.group(4))
        mb_per_sec = float(m1.group(5))
        ms_per_op = float(m1.group(6))

        metrics["IOSummary_ops"] = total_ops
        metrics["IOSummary_ops_per_sec"] = ops_per_sec
        metrics["IOSummary_rd_ops_per_sec"] = rd_ops_per_sec
        metrics["IOSummary_wr_ops_per_sec"] = wr_ops_per_sec
        metrics["IOSummary_mb_per_sec"] = mb_per_sec
        metrics["IOSummary_ms_per_op"] = ms_per_op

    elif m2:
        total_ops = int(m2.group(1))
        ops_per_sec = float(m2.group(2))
        rd_ops_per_sec = float(m2.group(3))
        wr_ops_per_sec = float(m2.group(4))
        mb_per_sec = float(m2.group(5))
        cpu_us_per_op = float(m2.group(6))
        latency_ms = float(m2.group(7))

        metrics["IOSummary_ops"] = total_ops
        metrics["IOSummary_ops_per_sec"] = ops_per_sec
        metrics["IOSummary_rd_ops_per_sec"] = rd_ops_per_sec
        metrics["IOSummary_wr_ops_per_sec"] = wr_ops_per_sec
        metrics["IOSummary_mb_per_sec"] = mb_per_sec
        metrics["IOSummary_cpu_us_per_op"] = cpu_us_per_op
        metrics["IOSummary_latency_ms"] = latency_ms

    # 패턴이 안 맞는 경우, 여기서 필요하면 print로 디버깅 가능
    return metrics


def main():
    parser = argparse.ArgumentParser(
        description="filebench 로그를 분석하여 Excel로 저장합니다."
    )
    parser.add_argument(
        "-i",
        "--input",
        default=DEFAULT_LOG_DIR,
        help=f"로그 디렉터리 (default: {DEFAULT_LOG_DIR})",
    )
    parser.add_argument(
        "-o",
        "--output",
        default=DEFAULT_OUTPUT,
        help=f"출력 XLSX 파일 (default: {DEFAULT_OUTPUT})",
    )

    args = parser.parse_args()

    log_dir = args.input
    output_path = args.output

    pattern = os.path.join(log_dir, "*.log")
    all_rows = []

    for path in sorted(glob.glob(pattern)):
        sched, bound, phase = parse_filename(path)
        row = {
            "sched": sched,
            "bound": bound,
            "phase": phase,
            "filename": os.path.basename(path),
        }
        row.update(parse_filebench_log(path))
        all_rows.append(row)

    if not all_rows:
        print(f"[!] 로그 없음: {log_dir}")
        return

    df = pd.DataFrame(all_rows)

    front_cols = ["sched", "bound", "phase", "filename"]
    metric_cols = [c for c in SELECTED_COLUMNS if c in df.columns]

    if not metric_cols:
        print("⚠ 선택한 컬럼이 없어서 모든 metric 사용.")
        metric_cols = [c for c in df.columns if c not in front_cols]

    final_cols = front_cols + metric_cols
    df = df[final_cols]

    os.makedirs(os.path.dirname(os.path.abspath(output_path)) or ".", exist_ok=True)

    with pd.ExcelWriter(output_path) as writer:
        for bound in sorted(df["bound"].unique()):
            sub = df[df["bound"] == bound].copy()
            sheet_name = bound[:31] if bound else "filebench"
            sub.to_excel(writer, sheet_name=sheet_name, index=False)

    print(f"[✓] 완료: {output_path}")
    print("📌 사용된 컬럼:")
    for col in final_cols:
        print(f"  - {col}")


if __name__ == "__main__":
    main()

