#!/usr/bin/env python3
import struct
from pathlib import Path
import multiprocessing as mp
import os


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


def decode_one_file(bin_path: Path, out_dir: Path):
    """
    解析单个 bitmap 文件（complete 或 timeout），
    输出人类可读的区间列表，并返回：
        (文件名, is_timeout, 指令数量)
    注意：这个函数会在子进程里跑。
    """
    is_timeout = "timeout" in bin_path.name
    out_name = bin_path.stem + "_decoded.txt"
    out_path = out_dir / out_name

    with bin_path.open("rb") as f:
        header = f.read(8)
        if len(header) != 8:
            raise ValueError(f"{bin_path} header too short")

        # 小端 int32 + int32
        file_number, range_count = struct.unpack("<ii", header)

        all_ranges = []

        for i in range(range_count):
            header_bytes = f.read(12)
            if len(header_bytes) != 12:
                raise ValueError(
                    f"{bin_path} range {i} header too short "
                    f"(expected 12 bytes, got {len(header_bytes)})"
                )

            start, end, size = struct.unpack("<III", header_bytes)

            bitmap = f.read(size)
            if len(bitmap) != size:
                raise ValueError(
                    f"{bin_path} range {i} bitmap too short "
                    f"(expected {size} bytes, got {len(bitmap)})"
                )

            ranges = bitmap_to_ranges(start, end, bitmap)
            all_ranges.extend(ranges)

    # 统计这个文件里一共有多少条指令被置 1
    insn_count = sum(e - s for (s, e) in all_ranges)

    # 写出 txt
    out_dir.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as out:
        kind = "TIMEOUT" if is_timeout else "EXEC"
        out.write(
            f"# file: {bin_path.name}, kind={kind}, "
            f"file_number={file_number}, ranges={range_count}\n"
        )
        out.write("# each line is [start, end) in hex\n\n")

        for s, e in all_ranges:
            out.write(f"[0x{s:08X}, 0x{e:08X}]\n")

    # 子进程返回本文件的统计信息
    return (bin_path.name, is_timeout, insn_count)


def _worker(args):
    """Pool.map 用的简单包装函数（可 picklable）"""
    bin_path, out_dir = args
    return decode_one_file(bin_path, out_dir)


def main():
    bitmap_dir = Path("bitmap_results")
    out_dir = Path("decoded_ranges")

    if not bitmap_dir.is_dir():
        print(f"bitmap directory not found: {bitmap_dir}")
        return

    bin_files = sorted(bitmap_dir.glob("res*_*.bin"))
    if not bin_files:
        print(f"no *.bin files found in {bitmap_dir}")
        return

    # 使用所有可用 CPU 核心（你的机器是 22 核，会自动用满）
    num_procs = os.cpu_count() or 1
    print(f"Using {num_procs} processes for decoding...")

    tasks = [(p, out_dir) for p in bin_files]

    # 多进程并行处理每个文件
    with mp.Pool(processes=num_procs) as pool:
        results = list(pool.map(_worker, tasks))

    # 汇总每个文件的指令数量
    per_file_exec = {}     # res*_complete.bin
    per_file_timeout = {}  # res*_timeout.bin

    for name, is_timeout, insn_count in results:
        if is_timeout:
            per_file_timeout[name] = insn_count
        else:
            per_file_exec[name] = insn_count

    total_exec = sum(per_file_exec.values())
    total_timeout = sum(per_file_timeout.values())

    # 写 summary.txt
    summary_path = out_dir / "summary.txt"
    with summary_path.open("w", encoding="utf-8") as f:
        f.write("# EXEC hidden instructions per file (res*_complete.bin)\n")
        for name, cnt in sorted(per_file_exec.items()):
            f.write(f"{name}: {cnt}\n")
        f.write(f"Total EXEC hidden instructions: {total_exec}\n\n")

        f.write("# TIMEOUT hidden instructions per file (res*_timeout.bin)\n")
        for name, cnt in sorted(per_file_timeout.items()):
            f.write(f"{name}: {cnt}\n")
        f.write(f"Total TIMEOUT hidden instructions: {total_timeout}\n")

    print("\nPer-file EXEC hidden instruction counts:")
    for name, cnt in sorted(per_file_exec.items()):
        print(f"  {name}: {cnt}")
    print(f"Total EXEC hidden instructions: {total_exec}")

    print("\nPer-file TIMEOUT hidden instruction counts:")
    for name, cnt in sorted(per_file_timeout.items()):
        print(f"  {name}: {cnt}")
    print(f"Total TIMEOUT hidden instructions: {total_timeout}")

    print(f"\nSummary written to {summary_path}")


if __name__ == "__main__":
    main()
