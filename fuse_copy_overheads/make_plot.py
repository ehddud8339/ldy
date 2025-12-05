#!/usr/bin/env python3
import argparse
import pandas as pd
import matplotlib.pyplot as plt

def main():
    parser = argparse.ArgumentParser(
        description="Plot queueing_ns and daemon_ns vs seq from CSV."
    )
    parser.add_argument("csv_path", help="input CSV file path")
    parser.add_argument(
        "-o", "--output",
        default="latency_vs_seq.png",
        help="output image file name (default: latency_vs_seq.png)"
    )
    args = parser.parse_args()

    # CSV 읽기
    df = pd.read_csv(args.csv_path)

    # 필요한 컬럼 있는지 체크
    for col in ["seq", "queueing_ns", "daemon_ns"]:
        if col not in df.columns:
            raise ValueError(f"CSV에 '{col}' 컬럼이 없습니다.")

    x = df["seq"]
    q = df["queueing_ns"]
    d = df["daemon_ns"]

    plt.figure(figsize=(10, 5))
    # 각 행의 값을 선 그래프(또는 포인트)로 표현
    plt.plot(x, q, label="queueing_ns", marker="o", linestyle="-")
    plt.plot(x, d, label="daemon_ns", marker="x", linestyle="-")

    plt.xlabel("seq")
    plt.ylabel("latency (ns)")
    plt.title("FUSE queueing vs daemon latency")
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(args.output, dpi=200)

    # GUI 없는 환경이면 show()는 생략해도 됨
    # plt.show()

if __name__ == "__main__":
    main()

