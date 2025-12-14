import pandas as pd
import argparse

def convert_csv_to_xlsx(csv_path, xlsx_path):
    # CSV 읽기
    df = pd.read_csv(csv_path)

    # ts_ns 기준 정렬
    df = df.sort_values(by="ts_ns")

    # riq_id 그룹화
    riq_groups = df.groupby("riq_id")

    # Excel 작성
    with pd.ExcelWriter(xlsx_path, engine="xlsxwriter") as writer:
        for riq_id, group in riq_groups:
            # READ / WRITE만 필터링
            group = group[group["opcode_name"].isin(["READ", "WRITE"])]
            # group = group[group["opcode_name"].isin(["GETATTR", "GETXATTR", "OPEN", "FLUSH", "RELEASE"])]
            if group.empty:
                continue

            # Sheet에 넣을 컬럼 구성
            out_df = group[[
                "ts_ns",
                "opcode_name",
                "queue_us",
                "daemon_us",
                "response_us",
                "copy_from_us",
                "copy_to_us"
            ]]

            sheet_name = f"riq_{riq_id}"
            out_df.to_excel(writer, sheet_name=sheet_name, index=False)

    print(f"[OK] XLSX saved to: {xlsx_path}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Convert CSV to XLSX (riq_id sheets).")
    parser.add_argument("--csv", required=True, help="Input CSV file path")
    parser.add_argument("--xlsx", required=True, help="Output XLSX file path")

    args = parser.parse_args()

    convert_csv_to_xlsx(args.csv, args.xlsx)

