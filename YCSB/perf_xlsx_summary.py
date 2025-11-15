#!/usr/bin/env python3
import os
import glob
import argparse
import pandas as pd


def parse_sched_bound(path: str):
    """
    파일 이름에서 scheduler와 bound 추출

    예:
      rr_mem_bound_perf_stat.log -> ('rr', 'mem_bound')
      cpu_workloada_perf_stat.log -> ('cpu', 'workloada')
    """
    base = os.path.basename(path)
    name = base.replace("_perf_stat.log", "")
    parts = name.split("_", 1)
    if len(parts) == 1:
        sched = parts[0]
        bound = "default"
    else:
        sched, bound = parts
    return sched, bound


def parse_perf_stat_log(path: str) -> pd.DataFrame:
    """
    perf stat -I 100 로그를 파싱해서
    time, <event1>, <event2>, ... 형태의 DataFrame으로 반환.

    예시 라인:
      1.000866118      4,827,978,183      L1-dcache-loads                                               (35.93%)
      1.000866118        479,148,721      L1-dcache-load-misses     #    9.92% of all L1-dcache accesses  (35.93%)

    - '<not supported>' 라인은 스킵
    - '# ...' 뒤 / '(...)' 뒤는 잘라냄
    """
    time_to_events = {}

    with open(path, "r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            # 헤더 라인 스킵
            if line.startswith("time") or line.startswith("#"):
                continue
            if "<not supported>" in line:
                continue

            # '# ...' 뒤는 주석이라 제거
            if "  #" in line:
                line = line.split("  #", 1)[0]
            # '(xx.xx%)' 같은 꼬리 제거
            if "(" in line:
                line = line.split("(", 1)[0]

            parts = line.split()
            if len(parts) < 3:
                continue

            # time
            try:
                t = float(parts[0])
            except ValueError:
                continue

            # counts (콤마 제거)
            cnt_str = parts[1].replace(",", "")
            try:
                count = int(cnt_str)
            except ValueError:
                # counts 자리에 다른 값이 오는 경우 방어
                continue

            # event 이름 (세 번째 토큰)
            event = parts[2]

            if t not in time_to_events:
                time_to_events[t] = {}
            time_to_events[t][event] = count

    if not time_to_events:
        return pd.DataFrame()

    rows = []
    for t in sorted(time_to_events.keys()):
        row = {"time": t}
        row.update(time_to_events[t])
        rows.append(row)

    df = pd.DataFrame(rows)
    return df


def build_bound_dataframes(log_dir: str):
    """
    log_dir 아래의 *perf_stat.log들을 bound 별로 분류해서
    bound -> DataFrame(time, sched1_event1, sched1_event2, ..., sched2_event1, ...) 구조로 반환
    """
    logs = sorted(glob.glob(os.path.join(log_dir, "**/*perf_stat.log"), recursive=True))

    if not logs:
        print("⚠️ No perf_stat logs found in:", log_dir)
        return {}

    by_bound = {}  # bound -> list of (sched, df)

    for path in logs:
        sched, bound = parse_sched_bound(path)
        df = parse_perf_stat_log(path)

        if df.empty:
            print(f"⚠️ No usable data in {path}, skipping.")
            continue

        if bound not in by_bound:
            by_bound[bound] = []
        by_bound[bound].append((sched, df))

    bound_dfs = {}

    for bound, entries in by_bound.items():
        merged = pd.DataFrame()

        for sched, df in entries:
            # time 컬럼은 그대로 두고, 이벤트 이름에 sched prefix 붙이기
            rename_map = {}
            for col in df.columns:
                if col == "time":
                    continue
                # 예: L1-dcache-loads -> rr_L1-dcache-loads
                rename_map[col] = f"{sched}_{col}"
            df_renamed = df.rename(columns=rename_map)

            if merged.empty:
                merged = df_renamed
            else:
                merged = pd.merge(merged, df_renamed, on="time", how="outer")

        merged = merged.sort_values("time").reset_index(drop=True)
        bound_dfs[bound] = merged

    return bound_dfs


def main():
    parser = argparse.ArgumentParser(
        description="Convert perf stat timeline logs to XLSX (one sheet per bound)."
    )
    parser.add_argument(
        "--input", "-i", required=True,
        help="Directory containing *perf_stat.log files"
    )
    parser.add_argument(
        "--output", "-o", required=True,
        help="Output XLSX file path"
    )

    args = parser.parse_args()
    input_dir = os.path.abspath(args.input)
    output_path = os.path.abspath(args.output)

    bound_dfs = build_bound_dataframes(input_dir)
    if not bound_dfs:
        return

    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)

    with pd.ExcelWriter(output_path) as writer:
        for bound, df in sorted(bound_dfs.items()):
            sheet_name = bound[:31]  # 엑셀 시트 이름 31자 제한
            df.to_excel(writer, sheet_name=sheet_name, index=False)

    print(f"\n✅ Export complete: {output_path}")
    print("📈 Each sheet = one bound, columns = time + sched_event counts.")


if __name__ == "__main__":
    main()
