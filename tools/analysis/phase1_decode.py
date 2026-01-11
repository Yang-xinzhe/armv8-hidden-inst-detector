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

def decode_one_file(args_tuple):
    """
    Reads a HIDR binary file and outputs a CSV file.
    """
    bin_path, csv_out_dir = args_tuple
    
    # Input: candidates_{N}_{type}.bin
    # Output: candidates_{N}_{type}.csv
    
    base_name = bin_path.stem
    csv_out_path = csv_out_dir / (base_name + ".csv")

    all_ranges = []
    
    with bin_path.open("rb") as f:
        header = f.read(16)
        if len(header) != 16:
            return 0
        
        magic, version, count, reserved = struct.unpack("<IIII", header)
        
        if magic != HIDR_MAGIC:
            print(f"Warning: {bin_path.name} has invalid magic {magic:x}")
            return 0
            
        for _ in range(count):
            range_bytes = f.read(8)
            if len(range_bytes) != 8:
                break
            start, end = struct.unpack("<II", range_bytes)
            all_ranges.append((start, end))

    insn_count = sum(e - s for (s, e) in all_ranges)

    if csv_out_dir:
        with csv_out_path.open("w", encoding="utf-8") as f:
            f.write("Start,End\n")
            for s, e in all_ranges:
                f.write(f"0x{s:X},0x{e:X}\n")

    return insn_count

def main():
    parser = argparse.ArgumentParser(description="Convert Phase 1 HIDR Binaries to CSV")
    parser.add_argument("-i", "--input", required=True, type=Path, help="Input directory containing candidates_*.bin files")
    parser.add_argument("--csv-out", required=True, type=Path, help="Output directory for CSV range files")
    
    args = parser.parse_args()

    if not args.input.is_dir():
        print(f"Error: Input directory {args.input} does not exist.")
        return

    args.csv_out.mkdir(parents=True, exist_ok=True)

    # Look for candidates_*.bin
    bin_files = sorted(args.input.glob("candidates_*.bin"))
    if not bin_files:
        print(f"No candidates_*.bin files found in {args.input}")
        return

    print(f"Processing {len(bin_files)} files from {args.input}...")
    
    tasks = [(p, args.csv_out) for p in bin_files]

    num_procs = os.cpu_count() or 1
    with mp.Pool(processes=num_procs) as pool:
        results = list(pool.map(decode_one_file, tasks))

    total_insns = sum(results)
    print(f"Done. Total instructions: {total_insns}")
    print(f"CSV ranges saved to: {args.csv_out}")

if __name__ == "__main__":
    main()
