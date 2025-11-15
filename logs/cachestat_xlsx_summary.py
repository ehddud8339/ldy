#!/usr/bin/env python3
import os
import glob
import argparse
import pandas as pd


def parse_sched_bound(path):
    """
    파일 이름에서 scheduler와 bound 추출

    예:
      rr_mem_bound_cachestat.log -> ('rr', 'mem_bound')
    """
    base = os.path.basename(path)
    name = base.replace("_cachestat.log", "")
    # rr_mem_bound_cachestat.log -> rr_mem_bound
    parts = name.split("_", 1)
    if len(parts) == 1:
        sched = parts[0]
        bound = "default"
    else:
        sched, bound = parts
    return sched, bound


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


def build_bound_dataframes(log_dir):
    """
    log_dir 아래의 *cachestat.log들을 bound 별로 분류해서
    bound -> DataFrame(time(sec), sched1, sched2, ...) 구조로 반환
    """
    logs = sorted(glob.glob(os.path.join(log_dir, "**/*cachestat.log"), recursive=True))

    if not logs:
        print("⚠️ No cachestat logs found in:", log_dir)
        return {}

    # bound -> list of (sched, values)
    by_bound = {}

    for path in logs:
        sched, bound = parse_sched_bound(path)
        data = parse_cachestat_log(path)

        if not data:
            print(f"⚠️ No usable data in {path}, skipping.")
            continue

        if bound not in by_bound:
            by_bound[bound] = []
        by_bound[bound].append((sched, data))

    # bound -> DataFrame
    bound_dfs = {}

    for bound, entries in by_bound.items():
        timeline_df = pd.DataFrame()

        for sched, values in entries:
            df = pd.DataFrame({"time(sec)": range(len(values)), sched: values})

            if timeline_df.empty:
                timeline_df = df
            else:
                timeline_df = pd.merge(timeline_df, df, on="time(sec)", how="outer")

        timeline_df = timeline_df.sort_values("time(sec)").reset_index(drop=True)
        bound_dfs[bound] = timeline_df

    return bound_dfs


def main():
    parser = argparse.ArgumentParser(
        description="Convert cachestat logs to XLSX timeline format (one sheet per bound)."
    )
    parser.add_argument("--input", "-i", required=True, help="Directory containing cachestat logs")
    parser.add_argument("--output", "-o", required=True, help="Output XLSX file path")

    args = parser.parse_args()

    bound_dfs = build_bound_dataframes(args.input)
    if not bound_dfs:
        return

    # 여러 sheet로 저장
    os.makedirs(os.path.dirname(os.path.abspath(args.output)) or ".", exist_ok=True)
    with pd.ExcelWriter(args.output) as writer:
        for bound, df in sorted(bound_dfs.items()):
            # bound 이름이 너무 길면 sheet 이름 제한(31자)에 걸릴 수 있음 → 슬라이스
            sheet_name = bound[:31]
            df.to_excel(writer, sheet_name=sheet_name, index=False)

    print(f"\n✅ Export complete: {args.output}")
    print("📈 Each sheet is one bound (io_bound, mem_bound, ...). Plot time(sec) vs sched columns as line charts.")


if __name__ == "__main__":
    main()
