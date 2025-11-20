#!/usr/bin/env python3
import re
import sys
from pathlib import Path

import pandas as pd


def parse_fio_log(path: Path, workload_name: str) -> dict:
    """
    fio.log에서 throughput, avg latency를 추출.
    - BW: 'read:' / 'write:' 라인에서 BW=... 부분
    - Avg Latency: 'clat' 또는 'lat' 라인에서 avg=... 부분
    """
    bw_value = None
    bw_unit = None
    lat_avg = None
    lat_unit = None

    if not path.is_file():
        return {
            "workload": workload_name,
            "bw_value": None,
            "bw_unit": None,
            "lat_avg": None,
            "lat_unit": None,
        }

    with path.open("r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()

    # BW 파싱:  read: / write: 라인
    bw_pattern = re.compile(r"BW=([\d\.]+)([KMG]i?B/s|\s*B/s|\s*kB/s|\s*MB/s|\s*GB/s)")
    # latency 파싱: lat (usec/msec) 혹은 clat (usec/msec)
    lat_pattern = re.compile(r"(?:clat|lat)\s*\((usec|msec|nsec)\).*avg=([\d\.]+)")

    for line in lines:
        if "read:" in line or "write:" in line:
            m = bw_pattern.search(line)
            if m:
                bw_value = float(m.group(1))
                bw_unit = m.group(2).strip()
        if "clat" in line or " lat (" in line:
            m2 = lat_pattern.search(line)
            if m2:
                lat_unit = m2.group(1)
                lat_avg = float(m2.group(2))

    return {
        "workload": workload_name,
        "bw_value": bw_value,
        "bw_unit": bw_unit,
        "lat_avg": lat_avg,
        "lat_unit": lat_unit,
    }


def parse_mpstat_log(path: Path, workload_name: str) -> pd.DataFrame:
    """
    mpstat -P ALL 1 로그 파싱.
    - time 컬럼 대신 단순 샘플 인덱스 t(초) 사용.
    - CPU, %usr, %nice, %sys, %iowait, %irq, %soft, %steal, %guest, %gnice, %idle
    """
    records = []

    if not path.is_file():
        return pd.DataFrame()

    with path.open("r", encoding="utf-8", errors="ignore") as f:
        lines = [ln.strip() for ln in f if ln.strip()]

    header = None
    sample_idx = -1

    for line in lines:
        # 헤더 찾기 (CPU %usr ... 포함)
        if "CPU" in line and "%usr" in line:
            header = [col for col in line.split() if col]
            continue

        if header is None:
            continue

        parts = line.split()
        # mpstat 출력 포맷이 locale/time 설정에 따라 달라질 수 있으므로
        # "CPU" 컬럼의 위치를 찾아서 그 뒤를 지표로 본다.
        try:
            cpu_idx = header.index("CPU")
        except ValueError:
            # 예외적인 헤더 형식이면 스킵
            continue

        # mpstat에 time column이 없을 수도 있으므로 CPU 칼럼 기준으로 정렬
        # ex) CPU %usr %nice ...
        # 혹은 12:34:56 AM  all ...
        if len(parts) < len(header):
            # time 컬럼이 앞에 끼어있는 경우가 대부분
            # CPU 토큰이 나오는 위치를 찾아 그 뒤를 매핑
            try:
                cpu_pos = parts.index("all") if "all" in parts else None
            except ValueError:
                cpu_pos = None

            if cpu_pos is None:
                # 포맷이 너무 다르면 그냥 스킵
                continue

            # parts[cpu_pos] ~ 를 header[CPU_idx:]에 붙인다
            data_tokens = parts[cpu_pos:]
        else:
            data_tokens = parts[: len(header)]

        if len(data_tokens) != len(header):
            continue

        cpu_val = data_tokens[cpu_idx]
        # sample index 증가: 첫 CPU(all)를 만날 때 t를 증가시키도록 할 수도 있지만
        # 여기서는 간단하게 모든 row에 대해 증가시키는 대신,
        # CPU=="all"일 때만 t++로 처리
        if cpu_val == "all":
            sample_idx += 1

        row = {
            "workload": workload_name,
            "t": sample_idx,
        }
        for h, v in zip(header, data_tokens):
            if h == "CPU":
                row["CPU"] = v
            else:
                # 숫자로 변환 시도
                try:
                    row[h] = float(v.replace(",", "."))
                except ValueError:
                    row[h] = None

        records.append(row)

    return pd.DataFrame.from_records(records)


def parse_iostat_log(path: Path, workload_name: str) -> pd.DataFrame:
    """
    iostat -x 1 -d <dev> 로그 파싱.
    - 디바이스 데이터 라인만 사용.
    - time index t(초) 부여.
    """
    if not path.is_file():
        return pd.DataFrame()

    records = []
    with path.open("r", encoding="utf-8", errors="ignore") as f:
        lines = [ln.rstrip() for ln in f if ln.strip()]

    header = None
    device_name = None
    sample_idx = -1

    for line in lines:
        # 헤더 찾기: Device, rrqm/s, wrqm/s, r/s, w/s ...
        if line.startswith("Device"):
            header = line.split()
            continue

        if header is None:
            continue

        parts = line.split()
        # 디바이스 이름은 첫 컬럼
        dev = parts[0]

        # 첫 데이터 줄에서 device_name 확정
        if device_name is None:
            device_name = dev
        # 다른 디바이스는 무시 (원래 -d nvme0n1 로 찍었으니 하나일 것)
        if dev != device_name:
            continue

        sample_idx += 1
        row = {
            "workload": workload_name,
            "t": sample_idx,
        }
        for h, v in zip(header, parts):
            if h == "Device":
                row["Device"] = v
            else:
                try:
                    row[h] = float(v.replace(",", "."))
                except ValueError:
                    row[h] = None

        records.append(row)

    return pd.DataFrame.from_records(records)


def parse_vmstat_log(path: Path, workload_name: str) -> pd.DataFrame:
    """
    vmstat 1 로그 파싱.
    - 앞의 두 블록(헤더 + 첫 요약)은 버리고,
      그 이후부터 1초 단위 샘플이라고 가정.
    - 기본 vmstat 컬럼 이름 사용.
    """
    if not path.is_file():
        return pd.DataFrame()

    with path.open("r", encoding="utf-8", errors="ignore") as f:
        lines = [ln.strip() for ln in f if ln.strip()]

    header = None
    data_lines = []
    header_found = False
    header_line_idx = None

    # vmstat 출력은 대략:
    # procs -----------memory---------- ---swap-- -----io---- -system-- ------cpu-----
    #  r  b   swpd   free   buff  cache   si   so    bi    bo   in   cs us sy id wa st
    #  1  0      0  ... (첫 샘플: 부팅 이후)
    #  0  0      0  ... (이후 1초 단위)
    for idx, line in enumerate(lines):
        if re.search(r"\br\b.*\bb\b.*\bswpd\b", line):
            header = line.split()
            header_line_idx = idx
            header_found = True
            break

    if not header_found or header_line_idx is None:
        return pd.DataFrame()

    # header_line 바로 다음 줄부터 데이터인데,
    # 첫 줄은 "부팅 후 평균"이라 보통 버리고, 그 다음부터 1초 단위로 사용.
    # 여기서는: header+1 = 첫 줄(평균), header+2부터 사용.
    for line in lines[header_line_idx + 2 :]:
        parts = line.split()
        # 컬럼 수가 다르면 스킵
        if len(parts) != len(header):
            continue
        data_lines.append(parts)

    records = []
    for t, parts in enumerate(data_lines):
        row = {
            "workload": workload_name,
            "t": t,
        }
        for h, v in zip(header, parts):
            try:
                row[h] = float(v.replace(",", "."))
            except ValueError:
                row[h] = None
        records.append(row)

    return pd.DataFrame.from_records(records)


def main(base_dir: str, output_xlsx: str):
    base = Path(base_dir).expanduser().resolve()
    if not base.is_dir():
        print(f"Base dir not found: {base}")
        sys.exit(1)

    # 하위 workload 디렉터리들
    workloads = sorted(
        [p for p in base.iterdir() if p.is_dir()],
        key=lambda p: p.name,
    )

    fio_rows = []
    mpstat_dfs = []
    iostat_dfs = []
    vmstat_dfs = []

    for wdir in workloads:
        workload_name = wdir.name  # 예: 4KB-rand-read
        fio_log = wdir / "fio.log"
        mpstat_log = wdir / "mpstat.log"
        iostat_log = wdir / "iostat.log"
        vmstat_log = wdir / "vmstat.log"

        # fio
        fio_rows.append(parse_fio_log(fio_log, workload_name))

        # mpstat
        df_mp = parse_mpstat_log(mpstat_log, workload_name)
        if not df_mp.empty:
            mpstat_dfs.append(df_mp)

        # iostat
        df_io = parse_iostat_log(iostat_log, workload_name)
        if not df_io.empty:
            iostat_dfs.append(df_io)

        # vmstat
        df_vm = parse_vmstat_log(vmstat_log, workload_name)
        if not df_vm.empty:
            vmstat_dfs.append(df_vm)

    fio_df = pd.DataFrame.from_records(fio_rows)

    mpstat_df = pd.concat(mpstat_dfs, ignore_index=True) if mpstat_dfs else pd.DataFrame()
    iostat_df = pd.concat(iostat_dfs, ignore_index=True) if iostat_dfs else pd.DataFrame()
    vmstat_df = pd.concat(vmstat_dfs, ignore_index=True) if vmstat_dfs else pd.DataFrame()

    # Excel로 저장
    out_path = Path(output_xlsx).expanduser().resolve()
    out_path.parent.mkdir(parents=True, exist_ok=True)

    with pd.ExcelWriter(out_path, engine="xlsxwriter") as writer:
        fio_df.to_excel(writer, sheet_name="fio", index=False)
        if not iostat_df.empty:
            iostat_df.to_excel(writer, sheet_name="iostat", index=False)
        if not vmstat_df.empty:
            vmstat_df.to_excel(writer, sheet_name="vmstat", index=False)
        if not mpstat_df.empty:
            mpstat_df.to_excel(writer, sheet_name="mpstat", index=False)

    print(f"Saved Excel: {out_path}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"사용법: {sys.argv[0]} <base_dir> <output.xlsx>")
        print("예:    parse_fio_logs.py ~/src/ldy/logs/fio/Ext4/512M  Ext4_512M_logs.xlsx")
        sys.exit(1)

    main(sys.argv[1], sys.argv[2])

