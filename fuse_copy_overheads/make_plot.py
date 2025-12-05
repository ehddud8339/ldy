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
    parser.add_argument(
        "--show",
        choices=["daemon", "queueing", "both"],
        default="both",
        help="Select which latency to show: daemon, queueing, or both"
    )
    parser.add_argument(
        "--ymax",
        type=float,
        default=None,
        help="Set maximum Y-axis value (default: auto-scale)"
    )

    args = parser.parse_args()

    # CSV 로드
    df = pd.read_csv(args.csv_path)

    # 필드 확인
    required = ["seq", "queueing_ns", "daemon_ns"]
    for col in required:
        if col not in df.columns:
            raise ValueError(f"CSV missing required column: {col}")

    x = df["seq"]

    plt.figure(figsize=(10, 5))

    if args.show in ("queueing", "both"):
        plt.plot(
            x, df["queueing_ns"],
            #label="queueing_ns",
            marker="o",
            markersize=2,
            linestyle="None"   # ← 선 제거
        )

    if args.show in ("daemon", "both"):
        plt.plot(
            x, df["daemon_ns"],
            label="daemon_ns",
            marker="x",
            markersize=2,
            linestyle="None"   # ← 선 제거
        )
    
    op_filter = df[df["opcode"].isin([16, 20])]

    for _, row in op_filter.iterrows():
        plt.axvline(
            x=row["seq"],
            color="red",
            linestyle="-",
            linewidth=0.8,
            alpha=0.6
        )

    plt.xlabel("seq")
    plt.ylabel("latency (ns)")
    plt.title("FUSE Queueing Delay")

    # y축 최대값 지정 여부
    if args.ymax is not None:
        plt.ylim(top=args.ymax)

    plt.grid(True)
    plt.legend()
    plt.tight_layout()

    plt.savefig(args.output, dpi=200)
    # plt.show()  # GUI 환경이면 사용 가능

if __name__ == "__main__":
    main()
