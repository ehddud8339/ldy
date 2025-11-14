#!/usr/bin/env python3
import os
import glob
import argparse
import pandas as pd


DEFAULT_LOG_DIR = "ycsb_20251114_"
DEFAULT_OUTPUT = "ycsb_summary.xlsx"


def parse_filename(path):
    """
    rr_cpu_bound_load.log  -> sched=rr, bound=cpu_bound, phase=load
    thr_io_bound_run.log   -> sched=thr, bound=io_bound, phase=run
    """
    base = os.path.basename(path)
    name, _ = os.path.splitext(base)
    parts = name.split("_")
    sched = parts[0]
    phase = parts[-1]              # load or run
    bound = "_".join(parts[1:-1])  # 중간 전부
    return sched, bound, phase


def parse_ycsb_log(path):
    """
    YCSB 결과 로그에서 [SECTION], Metric, Value 형태를 파싱해서 dict로 반환.
    예:
      [OVERALL], RunTime(ms), 1000
      [READ], AverageLatency(us), 123.45
    => key: "OVERALL_RunTime(ms)", "READ_AverageLatency(us)"
    """
    metrics = {}
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if not line or not line.startswith("[") or "," not in line:
                continue

            try:
                section_end = line.index("]")
                section = line[1:section_end]
                rest = line[section_end + 1:].lstrip(" ,")
                parts = [p.strip() for p in rest.split(",")]

                if len(parts) >= 2:
                    metric_name = parts[0]
                    value_str = parts[1]
                    key = f"{section}_{metric_name}"

                    try:
                        value = float(value_str) if "." in value_str else int(value_str)
                    except ValueError:
                        value = value_str

                    metrics[key] = value
            except Exception:
                continue

    return metrics


def main():
    parser = argparse.ArgumentParser(description="YCSB 로그를 분석하여 Excel로 저장합니다.")
    parser.add_argument("-i", "--input", default=DEFAULT_LOG_DIR,
                        help=f"로그 디렉터리 경로 (default: {DEFAULT_LOG_DIR})")
    parser.add_argument("-o", "--output", default=DEFAULT_OUTPUT,
                        help=f"출력 XLSX 파일 경로 (default: {DEFAULT_OUTPUT})")

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
        row.update(parse_ycsb_log(path))
        all_rows.append(row)

    if not all_rows:
        print(f"[!] 로그 파일을 찾지 못함: {log_dir}")
        return

    df = pd.DataFrame(all_rows)

    # 열 순서
    front_cols = ["sched", "bound", "phase", "filename"]
    df = df[front_cols + [c for c in df.columns if c not in front_cols]]

    df.to_excel(output_path, index=False)
    print(f"[✓] 완료: {output_path} 생성")


if __name__ == "__main__":
    main()

