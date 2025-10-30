import os
import re
from openpyxl import Workbook
from openpyxl.styles import Font, Alignment
from openpyxl.utils import get_column_letter

############################
# 실험 환경에 맞게 수정하는 부분
############################

BASE_LOG_DIR = "/home/ldy/src/ldy/rfuse/log_files/ext4"

WORKLOADS = ["read", "write", "randread", "randwrite"]
CPU_SETS = ["0-39", "0-19", "0-9", "0-4", "0"]  # 행 순서
NUMJOBS_LIST = ["1", "2", "4", "8", "16", "32"]  # 열 순서

# 출력 파일
OUTPUT_XLSX = "ext4_cpu_scaling_summary.xlsx"


############################
# fio 로그에서 값 추출하는 헬퍼
############################

def parse_fio_log(filepath, workload):
    """
    filepath: fio 결과 .log 경로
    workload: "read", "write", "randread", "randwrite"
              fio 로그 안에서 'read:' 'write:' 블록 중 어느 걸 볼지 결정할 때 사용
              randread -> read, randwrite -> write 라고 보면 됨.
    return: (iops, tail_99_99_usec)  # 둘 다 float
            없으면 None
    """

    # fio output의 read/write 구분:
    # randread도 fio 출력은 "read:" 섹션으로 나온다.
    # randwrite도 fio 출력은 "write:" 섹션으로 나온다.
    if workload in ["read", "randread"]:
        section_key = "read"
    else:
        section_key = "write"

    iops_val = None
    tail_val_usec = None

    # 우리는 clat percentiles 블록을 읽어서 99.99th를 잡을 건데,
    # 단위(usec, msec, nsec)를 함께 추적한다.
    clat_unit = None  # "nsec", "usec", "msec"

    with open(filepath, "r", errors="ignore") as f:
        for line in f:
            line_strip = line.strip()

            # 1) IOPS 파싱
            # 예: "   read: IOPS=12345, BW=..."
            # 또는 "  write: IOPS=12.3k, BW=..."
            # fio는 k/M/G 단위 접미사를 쓸 수 있다.
            m_iops = re.search(
                rf"{section_key}:\s+IOPS=([0-9]+(\.[0-9]+)?[kMG]?)",
                line_strip
            )
            if m_iops and iops_val is None:
                raw = m_iops.group(1)  # 예: '12.3k'
                # 접미사 처리
                mult = 1.0
                if raw.endswith('k'):
                    mult = 1e3
                    raw_num = raw[:-1]
                elif raw.endswith('M'):
                    mult = 1e6
                    raw_num = raw[:-1]
                elif raw.endswith('G'):
                    mult = 1e9
                    raw_num = raw[:-1]
                else:
                    raw_num = raw
                try:
                    iops_val = float(raw_num) * mult
                except ValueError:
                    pass

            # 2) clat percentiles 단위 감지
            # 예: "   clat percentiles (usec):"
            m_clat_header = re.search(r"clat percentiles \((nsec|usec|msec)\):", line_strip)
            if m_clat_header:
                clat_unit = m_clat_header.group(1)

            # 3) 99.99th latency 파싱
            # 예: "  | 99.99th=[  8450], 99.995th=[  9000], ..."
            m_p9999 = re.search(r"99\.99th=\[\s*([0-9]+)\s*\]", line_strip)
            if m_p9999 and tail_val_usec is None and clat_unit is not None:
                raw_tail = float(m_p9999.group(1))  # 숫자만
                # 단위 변환 → microseconds(usec) 기준으로 통합
                if clat_unit == "usec":
                    tail_val_usec = raw_tail
                elif clat_unit == "msec":
                    tail_val_usec = raw_tail * 1000.0
                elif clat_unit == "nsec":
                    tail_val_usec = raw_tail / 1000.0
                # else: leave None

    return iops_val, tail_val_usec


############################
# 엑셀 작성 유틸
############################

def write_workload_sheet(wb, workload, data_iops, data_tail):
    """
    wb: Workbook
    workload: "randread" 등 (시트 이름으로도 사용)
    data_iops[cpu_set][numjobs] = float or None
    data_tail[cpu_set][numjobs] = float(usec) or None

    시트 형식:
    [workload]_IOPS (col 1..7) | [workload]_99.99_tail_latency (col 9..15)
    각 블록의 1행은 병합된 헤더
    2행은 numjobs 헤더
    3행 이후는 cpu_set 행
    """

    ws = wb.create_sheet(title=workload)

    # 레이아웃 고정
    # 왼쪽 블록 시작열
    left_start_col = 1
    # 오른쪽 블록 시작열
    right_start_col = 1 + 1 + len(NUMJOBS_LIST) + 1  # 하나 비우고 다시 시작
    # 예: numjobs가 6개면 -> left 1..7 (A..G), 빈열 H, right는 I..O

    # 공용 스타일
    bold_center = Font(bold=True)
    center_align = Alignment(horizontal="center", vertical="center")

    # --- 왼쪽 블록 헤더: [workload]_IOPS ---
    left_header_col1 = left_start_col
    left_header_colN = left_start_col + len(NUMJOBS_LIST)  # merge across these
    ws.merge_cells(
        start_row=1,
        start_column=left_header_col1,
        end_row=1,
        end_column=left_header_colN
    )
    ws.cell(row=1, column=left_header_col1).value = f"[{workload}]_IOPS"
    ws.cell(row=1, column=left_header_col1).font = bold_center
    ws.cell(row=1, column=left_header_col1).alignment = center_align

    # 두 번째 행(numjobs row)
    for idx, nj in enumerate(NUMJOBS_LIST):
        col = left_start_col + idx
        ws.cell(row=2, column=col).value = nj
        ws.cell(row=2, column=col).font = bold_center
        ws.cell(row=2, column=col).alignment = center_align

    # 데이터 행들 (CPU 세트별)
    # row_offset = 3
    for r_i, cpu_set in enumerate(CPU_SETS):
        row = 3 + r_i
        # 왼쪽 첫 셀은 cpu_set 라벨을 IOPS 블록 왼쪽에 넣을지 말지는
        # 요구사항상 "cpus:0-39" 같은 문자열은 행 라벨처럼 보인다.
        # 표 예시에서 행 라벨은 맨 왼쪽에만 있고 IOPS/latency 블록 둘 다 공유하는지
        # 애매하지만, 여기서는 각 블록마다 넣지 않고 IOPS 쪽에만 넣자.
        row_label_col = left_start_col - 1
        if row_label_col >= 1:
            ws.cell(row=row, column=row_label_col).value = f"cpus:{cpu_set}"
            ws.cell(row=row, column=row_label_col).font = bold_center

        for idx, nj in enumerate(NUMJOBS_LIST):
            col = left_start_col + idx
            val = data_iops.get(cpu_set, {}).get(nj)
            ws.cell(row=row, column=col).value = val

    # --- 오른쪽 블록 헤더: [workload]_99.99_tail_latency ---
    right_header_col1 = right_start_col
    right_header_colN = right_start_col + len(NUMJOBS_LIST) - 1
    ws.merge_cells(
        start_row=1,
        start_column=right_header_col1,
        end_row=1,
        end_column=right_header_colN
    )
    ws.cell(row=1, column=right_header_col1).value = f"[{workload}]_99.99_tail_latency(usec)"
    ws.cell(row=1, column=right_header_col1).font = bold_center
    ws.cell(row=1, column=right_header_col1).alignment = center_align

    # 두 번째 행(numjobs row)
    for idx, nj in enumerate(NUMJOBS_LIST):
        col = right_start_col + idx
        ws.cell(row=2, column=col).value = nj
        ws.cell(row=2, column=col).font = bold_center
        ws.cell(row=2, column=col).alignment = center_align

    # 데이터 행들
    for r_i, cpu_set in enumerate(CPU_SETS):
        row = 3 + r_i
        # 오른쪽 블록도 행 라벨을 다시 적어주자. 가독성↑
        row_label_col = right_start_col - 1
        ws.cell(row=row, column=row_label_col).value = f"cpus:{cpu_set}"
        ws.cell(row=row, column=row_label_col).font = bold_center

        for idx, nj in enumerate(NUMJOBS_LIST):
            col = right_start_col + idx
            val = data_tail.get(cpu_set, {}).get(nj)
            ws.cell(row=row, column=col).value = val

    # 칸 정렬/너비 약간 손보기
    max_col = right_start_col + len(NUMJOBS_LIST)
    for c in range(1, max_col + 1):
        letter = get_column_letter(c)
        ws.column_dimensions[letter].width = 14


############################
# 메인 로직
############################

def main():
    # data 구조 초기화
    # data_iops[workload][cpu_set][numjobs] = float
    # data_tail[workload][cpu_set][numjobs] = float
    data_iops = {wl: {cs: {nj: None for nj in NUMJOBS_LIST} for cs in CPU_SETS} for wl in WORKLOADS}
    data_tail = {wl: {cs: {nj: None for nj in NUMJOBS_LIST} for cs in CPU_SETS} for wl in WORKLOADS}

    # 로그 디렉토리 순회
    # 기대 경로: BASE_LOG_DIR/cpu_0-39/*.log
    for cpu_set in CPU_SETS:
        cpu_dir = os.path.join(BASE_LOG_DIR, f"cpu_{cpu_set}")
        if not os.path.isdir(cpu_dir):
            continue

        for fname in os.listdir(cpu_dir):
            if not fname.endswith(".log"):
                continue

            # fname 예: randread_bs4K_nj32.log
            m = re.match(r"(read|write|randread|randwrite)_bs[^_]+_nj([0-9]+)\.log$", fname)
            if not m:
                continue

            workload = m.group(1)
            numjobs = m.group(2)

            if workload not in WORKLOADS:
                continue
            if numjobs not in NUMJOBS_LIST:
                continue

            fpath = os.path.join(cpu_dir, fname)
            iops, tail = parse_fio_log(fpath, workload)

            data_iops[workload][cpu_set][numjobs] = iops
            data_tail[workload][cpu_set][numjobs] = tail

    # 이제 엑셀 생성
    wb = Workbook()
    # 기본으로 생기는 첫 시트는 버린다 (openpyxl 기본 Sheet)
    default_ws = wb.active
    wb.remove(default_ws)

    for wl in WORKLOADS:
        write_workload_sheet(wb, wl, data_iops[wl], data_tail[wl])

    wb.save(OUTPUT_XLSX)
    print(f"Saved summary to {OUTPUT_XLSX}")


if __name__ == "__main__":
    main()

