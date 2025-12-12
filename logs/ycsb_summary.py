#!/usr/bin/env python3
import os
import glob
import argparse
import pandas as pd


DEFAULT_LOG_DIR = "ycsb_20251114_"
DEFAULT_OUTPUT = "ycsb_summary.xlsx"


# ================================
# ✨ 여기서 보고 싶은 컬럼을 지정하면 됨.
# 컬럼 이름은 원래 파싱 결과 키와 동일해야 함.
# 존재하지 않는 컬럼은 자동 무시됨.
# ================================
SELECTED_COLUMNS = [
    "OVERALL_Throughput(ops/sec)",
    #"OVERALL_RunTime(ms)",
    #"READ_AverageLatency(us)",
    #"READ_99thPercentileLatency(us)",
    #"UPDATE_AverageLatency(us)",
]
# ================================


def parse_filename(path):
    base = os.path.basename(path)
    name, _ = os.path.splitext(base)
    parts = name.split("_")
    sched = parts[0]
    phase = parts[-1]
    bound = "_".join(parts[1:-1])
    return sched, bound, phase


def parse_ycsb_log(path):
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
    parser.add_argument("-i", "--input", default=DEFAULT_LOG_DIR, help=f"로그 디렉터리 (default: {DEFAULT_LOG_DIR})")
    parser.add_argument("-o", "--output", default=DEFAULT_OUTPUT, help=f"출력 XLSX 파일 (default: {DEFAULT_OUTPUT})")
    
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
        print(f"[!] 로그 없음: {log_dir}")
        return

    df = pd.DataFrame(all_rows)

    # 앞에 항상 들어갈 메타 정보
    front_cols = ["sched", "bound", "phase", "filename"]

    # SELECTED_COLUMNS 중 실제로 존재하는 컬럼만 선택
    metric_cols = [c for c in SELECTED_COLUMNS if c in df.columns]

    # metric 없으면 전체 사용 (fallback)
    if not metric_cols:
        print("⚠ 선택한 컬럼이 없어서 모든 metric 사용.")
        metric_cols = [c for c in df.columns if c not in front_cols]

    final_cols = front_cols + metric_cols
    df = df[final_cols]

    os.makedirs(os.path.dirname(os.path.abspath(output_path)) or ".", exist_ok=True)

    with pd.ExcelWriter(output_path) as writer:
        for bound in sorted(df["bound"].unique()):
            sub = df[df["bound"] == bound].copy()
            sheet_name = bound[:31]
            sub.to_excel(writer, sheet_name=sheet_name, index=False)

    print(f"[✓] 완료: {output_path}")
    print("📌 사용된 컬럼:")
    for col in final_cols:
        print(f"  - {col}")


if __name__ == "__main__":
    main()

