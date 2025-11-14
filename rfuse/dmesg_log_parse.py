import re
from collections import Counter

LOG_FILE = "./log_files/rfuse/dmesg_log.txt"

# 정규식: 숫자 부분을 캡처
pattern = re.compile(
    r"current CPU core id=(\d+), selected ring channel id=(\d+)"
)

def analyze_fuse_log(file_path):
    total = 0
    mismatch = 0
    pair_counter = Counter()

    with open(file_path, "r", errors="ignore") as f:
        for line in f:
            m = pattern.search(line)
            if not m:
                continue
            total += 1
            core_id = int(m.group(1))
            ring_id = int(m.group(2))
            pair_counter[(core_id, ring_id)] += 1
            if core_id != ring_id:
                mismatch += 1

    print("=== FUSE Ring Channel Log Summary ===")
    print(f"Total entries           : {total}")
    print(f"Mismatch count          : {mismatch}")
    if total > 0:
        print(f"Mismatch ratio          : {mismatch / total * 100:.2f}%")
    print()

    print("Top (core_id, ring_id) pairs:")
    for (core, ring), cnt in pair_counter.most_common(10):
        print(f"  core={core:2d}, ring={ring:2d} -> {cnt} times")

if __name__ == "__main__":
    analyze_fuse_log(LOG_FILE)

