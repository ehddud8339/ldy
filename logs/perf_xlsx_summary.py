#!/usr/bin/env python3
import os
import glob
import argparse
import pandas as pd


DEFAULT_INPUT = "./"
DEFAULT_OUTPUT = "perf_timeline.xlsx"


def parse_filename(path: str):
    """
    예시 파일명:
      rr_cpu-pinned_perf_stat.log
      thr_io-bound_perf_stat.log

    → sched=rr, bound=cpu-pinned
    """
    base = os.path.basename(path)
    name, _ = os.path.splitext(base)

    # 뒤에 붙은 '_perf_stat' 제거
    if name.endswith("_perf_stat"):
        name = name[: -len("_perf_stat")]

    parts = name.split("_", 2)
    if len(parts) == 1:
        return parts[0], "default"
    elif len(parts) == 2:
        return parts[0], parts[1]
    else:
        # sched_bound_나머지 형태면 앞 2개만 사용
        return parts[0], parts[1]


def parse_perf_log(path: str):
    """
    perf stat -I 로그(단일 파일)를 파싱해서
      sec(정수 초), event, value
    형태의 long-format DataFrame으로 반환.

    같은 초(예: 1.0008, 1.9999)는 sec=1로 묶어서 합산한다.
    """
    records = []

    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.rstrip()
            if not line:
                continue
            if line.lstrip().startswith("#"):
                continue

            parts = line.split()
            # 기대 형식: time counts [unit] event ...
            if len(parts) < 3:
                continue

            # time (초) → 정수 초 버킷
            try:
                t = float(parts[0])
            except ValueError:
                continue

            sec = int(t)  # 같은 초는 모두 같은 버킷으로

            # counts (콤마 제거)
            try:
                count = float(parts[1].replace(",", ""))
            except ValueError:
                continue

            # 이벤트 이름: perf -I 기본 출력에서는 3번째 토큰이 이벤트 이름인 경우가 대부분
            event = parts[2]

            records.append({"sec": sec, "event": event, "value": count})

    if not records:
        return pd.DataFrame(columns=["sec", "event", "value"])

    df = pd.DataFrame(records)
    # 같은 sec, event가 중복일 경우 합산
    df = df.groupby(["sec", "event"], as_index=False)["value"].sum()
    return df


def build_bound_timeline(input_dir: str):
    """
    디렉터리 내의 *_perf_stat.log들을 읽어서

    bound 별로:
      time(sec), <sched1.event1>, <sched1.event2>, ..., <schedN.eventM>
    형태의 wide DataFrame 딕셔너리 반환.
    """
    pattern = os.path.join(input_dir, "*perf_stat.log")
    files = sorted(glob.glob(pattern))

    if not files:
        print(f"[WARN] No perf_stat logs found in {input_dir}")
        return {}

    bound_data = {}

    for path in files:
        sched, bound = parse_filename(path)
        df_long = parse_perf_log(path)
        if df_long.empty:
            print(f"[WARN] No usable data in {path}, skip.")
            continue

        # wide로 변환: sec × event, 값은 value
        df_pivot = df_long.pivot(index="sec", columns="event", values="value").sort_index()
        # 컬럼 이름 앞에 sched 붙여서 구분 (예: rr.cycles, thr.cycles)
        df_pivot.columns = [f"{sched}.{c}" for c in df_pivot.columns]

        if bound not in bound_data:
            bound_data[bound] = df_pivot
        else:
            # 기존 bound 데이터와 sec 기준 outer join
            bound_data[bound] = bound_data[bound].join(df_pivot, how="outer")

    # sec 컬럼을 앞으로 빼기 위해 reset_index
    for bound, df in bound_data.items():
        df_reset = df.reset_index().rename(columns={"sec": "time(sec)"})
        bound_data[bound] = df_reset

    return bound_data


def main():
    parser = argparse.ArgumentParser(description="Convert perf stat -I logs to timeline XLSX (bucket by second)")
    parser.add_argument(
        "-i", "--input",
        default=DEFAULT_INPUT,
        help=f"Input directory containing *perf_stat.log (default: {DEFAULT_INPUT})",
    )
    parser.add_argument(
        "-o", "--output",
        default=DEFAULT_OUTPUT,
        help=f"Output XLSX filename (default: {DEFAULT_OUTPUT})",
    )
    args = parser.parse_args()

    bound_dfs = build_bound_timeline(args.input)
    if not bound_dfs:
        return

    os.makedirs(os.path.dirname(os.path.abspath(args.output)) or ".", exist_ok=True)

    with pd.ExcelWriter(args.output) as writer:
        for bound, df in sorted(bound_dfs.items()):
            sheet_name = bound[:31]
            df.to_excel(writer, sheet_name=sheet_name, index=False)

    print(f"[✓] perf timeline saved → {args.output}")
    print("   - one sheet per bound (io_bound, cpu_bound, ...)")
    print("   - columns: time(sec), <sched>.<event> ..." )


if __name__ == "__main__":
    main()

