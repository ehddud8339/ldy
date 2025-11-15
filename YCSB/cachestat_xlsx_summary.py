#!/usr/bin/env python3
import os
import glob
import argparse
import pandas as pd


def parse_sched_name(path):
    """파일 이름에서 scheduler 이름 추출"""
    base = os.path.basename(path)
    name = base.replace("_cachestat.log", "")
    # 예: rr_mem_bound_cachestat.log → rr
    return name.split("_")[0]


def parse_cachestat_log(path):
    """cachestat 로그에서 HITRATIO만 추출"""
    values = []
    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line or "HITS" in line:
                continue
            parts = line.split()
            if len(parts) >= 4:
                try:
                    hit_ratio = float(parts[3].replace("%", ""))
                    values.append(hit_ratio)
                except ValueError:
                    continue
    return values


def build_dataframe(log_dir):
    timeline_df = pd.DataFrame()

    logs = sorted(glob.glob(os.path.join(log_dir, "**/*cachestat.log"), recursive=True))

    if not logs:
        print("⚠️ No cachestat logs found in:", log_dir)
        return None

    for path in logs:
        sched = parse_sched_name(path)
        data = parse_cachestat_log(path)

        if not data:
            print(f"⚠️ No usable data in {path}, skipping.")
            continue

        df = pd.DataFrame({"time(sec)": range(len(data)), sched: data})

        if timeline_df.empty:
            timeline_df = df
        else:
            timeline_df = pd.merge(timeline_df, df, on="time(sec)", how="outer")

    return timeline_df.sort_values("time(sec)").reset_index(drop=True)


def main():
    parser = argparse.ArgumentParser(description="Convert cachestat logs to XLSX timeline format.")
    parser.add_argument("--input", "-i", required=True, help="Directory containing cachestat logs")
    parser.add_argument("--output", "-o", required=True, help="Output XLSX file path")

    args = parser.parse_args()

    df = build_dataframe(args.input)
    if df is None:
        return

    df.to_excel(args.output, index=False)
    print(f"\n✅ Export complete: {args.output}")
    print("📈 Now open in Excel and plot a line chart.")


if __name__ == "__main__":
    main()

