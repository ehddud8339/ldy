#!/usr/bin/env python3
import os
import argparse
import glob

# 관심 있는 지표 목록
METRICS = [
    "pgfault", "pgmajfault",
    "nr_dirty", "nr_writeback",
    "pgpgin", "pgpgout",
    "numa_hit", "numa_miss", "numa_local", "numa_other", "numa_interleave",
    "nr_dirtied", "nr_written",
    "pswpin", "pswpout",
    "allocstall_dma", "allocstall_dma32", "allocstall_normal", "allocstall_movable"
]

PAGE_SIZE = 1024  # pgpgin/pgpgout 단위


def read_vmstat_file(path):
    data = {}
    with open(path, "r") as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) == 2 and parts[1].isdigit():
                data[parts[0]] = int(parts[1])
    return data


def diff_vmstat(before, after):
    diff = {}
    keys = set(before.keys()) | set(after.keys())
    for key in keys:
        diff[key] = after.get(key, 0) - before.get(key, 0)
    return diff


def process_pair(before_path, after_path, summary_path):
    before = read_vmstat_file(before_path)
    after = read_vmstat_file(after_path)
    diff = diff_vmstat(before, after)

    lines = []

    for name in METRICS:
        if name not in diff:
            continue

        val = diff[name]
        lines.append(f"{name} {val}")

        # pgpgin/pgpgout은 바이트 환산 값도 기록
        if name in ("pgpgin", "pgpgout"):
            lines.append(f"{name}_bytes {val * PAGE_SIZE}")

    with open(summary_path, "w") as f:
        f.write("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description="Process vmstat before/after logs and generate summary.")
    parser.add_argument("--input", required=True, help="Directory containing *_vmstat_before.log and *_vmstat_after.log")
    parser.add_argument("--output", required=True, help="Directory where summary logs will be written")

    args = parser.parse_args()

    input_dir = os.path.abspath(args.input)
    output_dir = os.path.abspath(args.output)

    if not os.path.exists(input_dir):
        print(f"[ERROR] Input directory does not exist: {input_dir}")
        return

    os.makedirs(output_dir, exist_ok=True)

    before_files = glob.glob(os.path.join(input_dir, "*_vmstat_before.log"))

    if not before_files:
        print(f"[ERROR] No *_vmstat_before.log found in {input_dir}")
        return

    for before_path in sorted(before_files):
        base = os.path.basename(before_path).replace("_vmstat_before.log", "")
        after_path = os.path.join(input_dir, base + "_vmstat_after.log")

        if not os.path.exists(after_path):
            print(f"[SKIP] No matching after log for: {before_path}")
            continue

        summary_path = os.path.join(output_dir, base + "_vmstat_summary.log")

        print(f"[PROCESS] {base} -> {summary_path}")
        process_pair(before_path, after_path, summary_path)


if __name__ == "__main__":
    main()

