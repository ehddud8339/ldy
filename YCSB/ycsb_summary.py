#!/usr/bin/env python3
import os
import glob
import pandas as pd

LOG_DIR = "YCSB\ycsb_20251114_035424"          # 로그가 있는 디렉터리 경로 (필요하면 수정)
OUTPUT_XLSX = "ycsb_summary.xlsx"


def parse_filename(path):
    """
    rr_cpu_bound_load.log  -> sched=rr, bound=cpu_bound, phase=load
    thr_io_bound_run.log   -> sched=thr, bound=io_bound,  phase=run
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
    YCSB 결과 로그에서 [섹션], Metric, Value 형태를 파싱해서 dict로 반환.
    예:
      [OVERALL], RunTime(ms), 1000
      [READ], AverageLatency(us), 123.45
    => key: "OVERALL_RunTime(ms)", "READ_AverageLatency(us)"
    """
    metrics = {}
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            # YCSB 형식 라인만 처리
            if line.startswith("[") and "]" in line and "," in line:
                # 예: [OVERALL], RunTime(ms), 1000
                try:
                    section_end = line.index("]")
                    section = line[1:section_end]  # OVERALL, READ, UPDATE 등
                    rest = line[section_end + 1 :].lstrip(" ,")
                    parts = [p.strip() for p in rest.split(",")]
                    if len(parts) >= 2:
                        metric_name = parts[0]      # RunTime(ms)
                        value_str = parts[1]        # 1000
                        key = f"{section}_{metric_name}"
                        # 숫자 변환 시도
                        try:
                            if "." in value_str:
                                value = float(value_str)
                            else:
                                value = int(value_str)
                        except ValueError:
                            value = value_str
                        metrics[key] = value
                except Exception:
                    # 이상한 라인은 그냥 무시
                    continue
    return metrics


def main():
    all_rows = []

    pattern = os.path.join(LOG_DIR, "*.log")
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
        print("로그 파일을 찾지 못했습니다.")
        return

    df = pd.DataFrame(all_rows)

    # 열 순서 조금 정리
    front_cols = ["sched", "bound", "phase", "filename"]
    other_cols = [c for c in df.columns if c not in front_cols]
    df = df[front_cols + other_cols]

    df.to_excel(OUTPUT_XLSX, index=False)
    print(f"완료: {OUTPUT_XLSX} 생성")


if __name__ == "__main__":
    main()

