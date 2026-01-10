#!/usr/bin/env python3
import struct
import argparse
from pathlib import Path
import multiprocessing as mp
import os
import sys

# Magic: 'HIDR' (Hidden Inst Detector Ranges) -> 0x52444948 (Little Endian)
HIDR_MAGIC = 0x52444948
HIDR_VERSION = 1

def bitmap_to_ranges(start, end, bitmap_bytes):
    """
    把一个区间 [start, end) 内的位图还原成若干连续的指令区间。
    返回 [(range_start, range_end), ...]，同样是半开区间。
    """
    bits = end - start
    ranges = []
    cur_start = None

    for offset in range(bits):
        byte_index = offset // 8
        bit_pos = offset % 8
        bit_set = (bitmap_bytes[byte_index] >> bit_pos) & 1

        if bit_set:
            if cur_start is None:
                cur_start = start + offset
        else:
            if cur_start is not None:
                ranges.append((cur_start, start + offset))
                cur_start = None

    if cur_start is not None:
        ranges.append((cur_start, end))

    return ranges


def decode_one_file(args_tuple):
    """
    解析单个 bitmap 文件，输出 .bin 和 .csv
    注意：这个函数会在子进程里跑。
    """
    bin_path, bin_out_dir, csv_out_dir = args_tuple
    
    # 解析文件名中的 file_number (假设格式 resX_*.bin)
    # 这里的 file_number 最好从文件头读，或者从文件名解
    stem = bin_path.stem
    # 提取数字部分，例如 res123_complete -> 123
    try:
        # 简单粗暴提取：找到第一个数字序列
        import re
        match = re.search(r'res(\d+)_', bin_path.name)
        if match:
            file_num_from_name = int(match.group(1))
        else:
            # Fallback
            file_num_from_name = 0 
    except:
        file_num_from_name = 0

    is_timeout = "timeout" in bin_path.name
    # 只处理 complete 文件用于生成 candidate，timeout 文件通常意味着不确定性，
    # 但如果策略是只要非 crash 就测，那 timeout 也应该包含。
    # 这里保持原逻辑：解码所有 bin 文件。
    
    # 输出文件名规范：candidates_{file_num}.bin / .csv
    # 如果源文件区分了 complete/timeout，我们也应该区分，或者合并？
    # 原逻辑是 decode 所有的。
    # 为了防止文件名冲突，保留原有的 suffix
    suffix = "_timeout" if is_timeout else "_complete"
    base_name = f"candidates_{file_num_from_name}{suffix}"
    
    bin_out_path = bin_out_dir / (base_name + ".bin")
    csv_out_path = csv_out_dir / (base_name + ".csv")

    all_ranges = []
    
    # 1. 读取并解析 Bitmap
    with bin_path.open("rb") as f:
        header = f.read(8)
        if len(header) != 8:
            return 0
        
        # Bitmap Header: file_number(4), range_count(4)
        file_number, range_count = struct.unpack("<ii", header)

        for _ in range(range_count):
            header_bytes = f.read(12)
            if len(header_bytes) < 12: break
            
            start, end, size = struct.unpack("<III", header_bytes)
            bitmap = f.read(size)
            if len(bitmap) != size:
                # Handle partial read if necessary, or just break
                if bitmap:
                    ranges = bitmap_to_ranges(start, start + len(bitmap) * 8, bitmap)
                    all_ranges.extend(ranges)
                break

            ranges = bitmap_to_ranges(start, end, bitmap)
            all_ranges.extend(ranges)

    insn_count = sum(e - s for (s, e) in all_ranges)
    
    # 2. 写入二进制格式 (HIDR Format)
    # Header: Magic(4) + Version(4) + Count(4) + Reserved(4)
    # Body: [Start(4) + End(4)] * Count
    if bin_out_dir:
        with bin_out_path.open("wb") as f:
            header = struct.pack("<IIII", HIDR_MAGIC, HIDR_VERSION, len(all_ranges), 0)
            f.write(header)
            for s, e in all_ranges:
                f.write(struct.pack("<II", s, e))

    # 3. 写入 CSV 格式
    # Start, End (Hex or Dec? Hex is better for humans)
    if csv_out_dir:
        with csv_out_path.open("w", encoding="utf-8") as f:
            f.write("Start,End\n")
            for s, e in all_ranges:
                f.write(f"0x{s:X},0x{e:X}\n")

    return insn_count


def main():
    parser = argparse.ArgumentParser(description="Decode Phase 1 Bitmaps to Binary/CSV Ranges")
    parser.add_argument("-i", "--input", required=True, type=Path, help="Input directory containing .bin bitmap files")
    parser.add_argument("--bin-out", type=Path, help="Output directory for binary range files (.bin)")
    parser.add_argument("--csv-out", type=Path, help="Output directory for CSV range files (.csv)")
    
    args = parser.parse_args()

    if not args.input.is_dir():
        print(f"Error: Input directory {args.input} does not exist.")
        return

    # Default output dirs if not specified
    if not args.bin_out and not args.csv_out:
        print("Error: At least one output directory (--bin-out or --csv-out) must be specified.")
        return

    if args.bin_out:
        args.bin_out.mkdir(parents=True, exist_ok=True)
    if args.csv_out:
        args.csv_out.mkdir(parents=True, exist_ok=True)

    bin_files = sorted(args.input.glob("res*_*.bin"))
    if not bin_files:
        print(f"No .bin files found in {args.input}")
        return

    print(f"Processing {len(bin_files)} files from {args.input}...")
    
    # Prepare arguments for worker
    tasks = [(p, args.bin_out, args.csv_out) for p in bin_files]

    num_procs = os.cpu_count() or 1
    with mp.Pool(processes=num_procs) as pool:
        results = list(pool.map(decode_one_file, tasks))

    total_insns = sum(results)
    print(f"Done. Total decoded instructions: {total_insns}")
    if args.bin_out:
        print(f"Binary ranges saved to: {args.bin_out}")
    if args.csv_out:
        print(f"CSV ranges saved to:    {args.csv_out}")


if __name__ == "__main__":
    main()
