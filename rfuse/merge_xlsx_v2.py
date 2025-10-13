
import sys
from pathlib import Path
import re
from collections import defaultdict, OrderedDict

from openpyxl import load_workbook, Workbook
from openpyxl.styles import Alignment, Font, Border, Side

TARGET_METRICS = ["IOPS", "BW(MB/s)", "lat_avg"]
WORKLOAD_ORDER = ["randread", "randwrite", "read", "write"]
CPU_STATE_MAP = {
    "0,2,4,6": "BUSY",
    "8,10,12,14": "IDLE",
}

def fs_name_from_path(p: Path) -> str:
    stem = p.stem  # e.g., "ext4_summary"
    m = re.match(r"(.+?)_summary$", stem, re.IGNORECASE)
    return m.group(1) if m else stem

def parse_tables_from_sheet(ws):
    """
    Parse tables from the 'summary' sheet generated earlier.
    Returns dict: {(cpus, workload, bs): {metric: [v1,v2,v3,v4]}}
    """
    max_row = ws.max_row
    cur = 1
    out = {}

    while cur <= max_row:
        head = ws.cell(cur, 2).value
        nj_ok = (
            cur + 1 <= max_row and
            ws.cell(cur+1, 2).value == 1 and
            ws.cell(cur+1, 3).value == 2 and
            ws.cell(cur+1, 4).value == 3 and
            ws.cell(cur+1, 5).value == 4
        )

        if isinstance(head, str) and nj_ok:
            vals = {}
            for i, metric in enumerate(TARGET_METRICS, start=2):
                row_vals = [ws.cell(cur + i, c).value for c in range(2, 6)]
                vals[metric] = row_vals

            parts = head.split("_")
            if len(parts) >= 3:
                cpus = parts[0]
                workload = parts[1]
                bs = parts[2]
                out[(cpus, workload, bs)] = vals

            # skip header + numjobs row + 3 metric rows + 3 spacing rows
            cur += 1 + 1 + len(TARGET_METRICS) + 3
            continue
        else:
            cur += 1

    return out

def bs_key(bs: str):
    m = re.match(r"(\d+)\s*k$", bs.strip(), re.IGNORECASE)
    return int(m.group(1)) if m else 10**9

def workload_key(w):
    try:
        return WORKLOAD_ORDER.index(w)
    except ValueError:
        return len(WORKLOAD_ORDER) + (ord(w[0]) if w else 0)

def build_output(data_by_fs: dict, fs_order_pref=None):
    """
    data_by_fs: {fs_name: {(cpus, workload, bs): {metric: [v1..v4]}}}
    We want to produce tables keyed by (workload, bs), with rows BUSY_<fs>, IDLE_<fs>.
    Returns:
      keys: sorted list of (workload, bs)
      table_map[(workload,bs,metric)][state][fs] = [v1..v4]
      fs_order: ordered list of fs names
    """
    # Collect all workload/bs pairs that appear with BUSY or IDLE cpus keys
    wb_keys = set()
    for fs, d in data_by_fs.items():
        for (cpus, workload, bs) in d.keys():
            if cpus in CPU_STATE_MAP:
                wb_keys.add((workload, bs))

    keys = sorted(wb_keys, key=lambda k: (workload_key(k[0]), bs_key(k[1])))

    # Prepare table_map
    table_map = defaultdict(lambda: defaultdict(dict))  # -> [ (workload,bs,metric) ][state][fs] -> list[4]
    for fs, d in data_by_fs.items():
        for (workload, bs) in keys:
            for metric in TARGET_METRICS:
                # Initialize both states with None if missing
                for state in CPU_STATE_MAP.values():
                    table_map[(workload, bs, metric)].setdefault(state, {})
                    table_map[(workload, bs, metric)][state].setdefault(fs, [None]*4)

            # For cpus variants present for this fs/workload/bs, fill per-state rows
            for (cpus, w2, b2), metrics in d.items():
                if w2 != workload or b2 != bs or cpus not in CPU_STATE_MAP:
                    continue
                state = CPU_STATE_MAP[cpus]
                for metric in TARGET_METRICS:
                    table_map[(workload, bs, metric)][state][fs] = metrics.get(metric, [None]*4)

    # Determine FS order
    all_fs = list(data_by_fs.keys())
    if fs_order_pref:
        fs_order = [fs for fs in fs_order_pref if fs in all_fs]
        fs_order += [fs for fs in sorted(all_fs) if fs not in fs_order]
    else:
        fs_order = sorted(all_fs)

    return keys, table_map, fs_order

def write_output(keys, table_map, fs_order, out_path: Path):
    wb = Workbook()
    ws = wb.active
    ws.title = "summary"

    center = Alignment(horizontal="center", vertical="center")
    right = Alignment(horizontal="right", vertical="center")
    bold = Font(bold=True)
    thin = Side(style="thin", color="000000")
    border = Border(left=thin, right=thin, top=thin, bottom=thin)

    current_row = 1
    tables_written = 0

    for (workload, bs) in keys:
        for metric in TARGET_METRICS:
            header = f"{workload}_{bs}_{metric}"
            # Header row (merge B..E)
            ws.merge_cells(start_row=current_row, start_column=2, end_row=current_row, end_column=5)
            h = ws.cell(current_row, 2, header)
            h.font = bold; h.alignment = center
            current_row += 1

            # numjobs header
            for i, nj in enumerate([1,2,3,4], start=2):
                c = ws.cell(current_row, i, nj)
                c.font = bold; c.alignment = center; c.border = border
            current_row += 1

            # Rows: BUSY_<fs> then IDLE_<fs>
            for state in ["BUSY", "IDLE"]:
                for fs in fs_order:
                    row_name = f"{state}_{fs}"
                    ws.cell(current_row, 1, row_name).font = bold
                    ws.cell(current_row, 1).alignment = right
                    ws.cell(current_row, 1).border = border

                    vals = table_map[(workload, bs, metric)][state].get(fs, [None]*4)
                    for i, v in enumerate(vals, start=2):
                        c = ws.cell(current_row, i, v)
                        c.alignment = center; c.border = border
                    current_row += 1

            # spacing
            current_row += 3
            tables_written += 1

    # Adjust widths
    ws.column_dimensions["A"].width = 18
    for col in ["B","C","D","E"]:
        ws.column_dimensions[col].width = 12

    out_path.parent.mkdir(parents=True, exist_ok=True)
    wb.save(out_path)
    print(f"[OK] Wrote {tables_written} tables -> {out_path}")

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 merge_fs_summaries_v2.py <fs1_summary.xlsx> [<fs2_summary.xlsx> ...] [--out OUTPUT.xlsx]")
        print("FS name is derived from filename prefix before '_summary.xlsx' (e.g., 'ext4_summary.xlsx' -> 'ext4').")
        sys.exit(1)

    args = sys.argv[1:]
    if "--out" in args:
        idx = args.index("--out")
        out_path = Path(args[idx+1])
        files = [Path(p) for p in (args[:idx] + args[idx+2:])]
    else:
        files = [Path(p) for p in args]
        out_path = files[0].parent / "merged_summary_v2.xlsx"

    data_by_fs = {}
    for f in files:
        fs = fs_name_from_path(f)
        wb = load_workbook(f, data_only=True)
        if "summary" not in wb.sheetnames:
            print(f"[WARN] {f} has no 'summary' sheet. Skipped.")
            continue
        ws = wb["summary"]
        data_by_fs[fs] = parse_tables_from_sheet(ws)

    if not data_by_fs:
        print("[ERROR] No data parsed from inputs.")
        sys.exit(2)

    fs_pref = ["ext4", "fuse", "rfuse"]
    keys, table_map, fs_order = build_output(data_by_fs, fs_order_pref=fs_pref)
    write_output(keys, table_map, fs_order, out_path)

if __name__ == "__main__":
    main()

