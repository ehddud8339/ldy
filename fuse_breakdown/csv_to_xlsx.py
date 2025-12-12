import pandas as pd
import argparse
import math

def csv_to_xlsx_rw(csv_path: str, xlsx_path: str, chunk_size=1_000_000):
    # CSV 읽기
    df = pd.read_csv(csv_path)

    # op가 READ 또는 WRITE 인 것만 필터링
    df = df[df["op"].isin(["READ", "WRITE"])]
    #df = df[df["op"].isin(["GETATTR", "OPEN", "FLUSH", "RELEASE", "LOOKUP"])]

    # 필요한 컬럼만 선택
    out_df = df[["unique", "queuing_us", "daemon_us", "response_us"]]

    total_rows = len(out_df)
    num_chunks = math.ceil(total_rows / chunk_size)

    print(f"[INFO] total rows = {total_rows}, chunks = {num_chunks}")

    # XLSX로 저장 (여러 시트 자동 분할)
    with pd.ExcelWriter(xlsx_path, engine="xlsxwriter") as writer:
        for i in range(num_chunks):
            start = i * chunk_size
            end = min(start + chunk_size, total_rows)
            chunk = out_df.iloc[start:end]

            sheet_name = f"READ_WRITE_{i+1}"
            print(f"[INFO] writing {sheet_name} ({len(chunk)} rows)...")

            chunk.to_excel(writer, sheet_name=sheet_name, index=False)

    print(f"[OK] saved: {xlsx_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Convert CSV to XLSX (READ/WRITE rows only, auto-split if too large)."
    )
    parser.add_argument("--csv", required=True, help="Input CSV file path")
    parser.add_argument("--xlsx", required=True, help="Output XLSX file path")

    args = parser.parse_args()
    csv_to_xlsx_rw(args.csv, args.xlsx)

