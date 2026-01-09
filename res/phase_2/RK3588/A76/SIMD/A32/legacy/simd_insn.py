import os
import struct
import sys
import glob

def merge_ranges(indices):
    """
    将连续的索引合并为 [start, end) 区间
    """
    if not indices:
        return []
    
    ranges = []
    range_start = indices[0]
    prev = indices[0]
    
    for curr in indices[1:]:
        if curr != prev + 1:
            # 这里的 end 是开区间，与 C 代码中的 [start, end) 保持一致
            ranges.append((range_start, prev + 1))
            range_start = curr
        prev = curr
    
    ranges.append((range_start, prev + 1))
    return ranges

def process_file(filepath):
    """
    返回: (count_simd, ranges_simd, count_fpscr, ranges_fpscr, count_all, ranges_all)
    """
    ranges_simd = []
    ranges_fpscr = []
    ranges_all = []
    
    cnt_simd = 0
    cnt_fpscr = 0
    cnt_all = 0
    
    try:
        filesize = os.path.getsize(filepath)
        if filesize == 0:
            return 0, [], 0, [], 0, []

        with open(filepath, 'rb') as f:
            # 读取文件头: file_number (4 bytes), range_count (4 bytes)
            header_data = f.read(8)
            if len(header_data) < 8:
                print(f"Warning: File {filepath} is too short for header")
                return 0, [], 0, [], 0, []
            
            file_num, range_count = struct.unpack('<ii', header_data)
            
            for _ in range(range_count):
                # 读取 Range Header: start(4), end(4), size(4)
                range_header = f.read(12)
                if len(range_header) < 12:
                    break
                
                start, end, size = struct.unpack('<III', range_header)
                num_bits = end - start
                
                # simd.c 中初始化的 mask 为 RB_MASK_SIMD | RB_MASK_FPSCR
                # 根据 inc/bitmap.h 中的 RB_PLANE 定义:
                # GPR=2, CPSR=3, LD=4, ST=5, SP=6, SIMD=7, FPSCR=8
                # serialize 函数会按顺序写入分配了的 plane。
                # 我们期望读取到两个 bitmap block：先 SIMD，后 FPSCR。
                
                # 读取 SIMD bitmap
                simd_bitmap = f.read(size)
                if len(simd_bitmap) != size:
                    print(f"Warning: Unexpected EOF reading SIMD bitmap in {filepath}")
                    break
                    
                # 读取 FPSCR bitmap
                fpscr_bitmap = f.read(size)
                if len(fpscr_bitmap) != size:
                    print(f"Warning: Unexpected EOF reading FPSCR bitmap in {filepath}")
                    break
                
                # 解析位图
                indices_simd_local = set()
                indices_fpscr_local = set()
                indices_all_local = set()
                
                for i in range(num_bits):
                    byte_idx = i // 8
                    bit_idx = i % 8
                    
                    is_simd = (simd_bitmap[byte_idx] >> bit_idx) & 1
                    is_fpscr = (fpscr_bitmap[byte_idx] >> bit_idx) & 1
                    
                    if is_simd:
                        indices_simd_local.add(i)
                    if is_fpscr:
                        indices_fpscr_local.add(i)
                    if is_simd or is_fpscr:
                        indices_all_local.add(i)
                
                # 处理 SIMD
                if indices_simd_local:
                    abs_simd = sorted([start + i for i in indices_simd_local])
                    cnt_simd += len(abs_simd)
                    ranges_simd.extend(merge_ranges(abs_simd))

                # 处理 FPSCR
                if indices_fpscr_local:
                    abs_fpscr = sorted([start + i for i in indices_fpscr_local])
                    cnt_fpscr += len(abs_fpscr)
                    ranges_fpscr.extend(merge_ranges(abs_fpscr))

                # 处理 Union (All)
                if indices_all_local:
                    abs_all = sorted([start + i for i in indices_all_local])
                    cnt_all += len(abs_all)
                    ranges_all.extend(merge_ranges(abs_all))
                    
    except Exception as e:
        print(f"Error processing {filepath}: {e}")
        import traceback
        traceback.print_exc()
        return 0, [], 0, [], 0, []

    return cnt_simd, ranges_simd, cnt_fpscr, ranges_fpscr, cnt_all, ranges_all

def analyze_cpsr_logs(input_dir, output_file):
    """
    Parse res*_cpsr.bin files (for SIMD, this might be FPSCR logs, but filename in simd.c is res%d_cpsr.bin) 
    and generate a detailed report.
    Struct: FpsrChangeLog { uint32_t insn, uint32_t before, uint32_t after }
    """
    pattern = os.path.join(input_dir, "res*_cpsr.bin")
    files = glob.glob(pattern)
    files.sort()
    
    try:
        files.sort(key=lambda x: int(os.path.basename(x).replace('res', '').replace('_cpsr.bin', '')))
    except:
        pass

    if not files:
        print(f"No FPSCR log files found in {input_dir}")
        return

    with open(output_file, "w") as out:
        out.write("FPSCR Change Detail Report\n")
        out.write("==========================\n\n")

        total_changes = 0
        
        for filepath in files:
            filename = os.path.basename(filepath)
            out.write(f"--- File: {filename} ---\n")
            
            try:
                with open(filepath, "rb") as f:
                    while True:
                        data = f.read(12)
                        if len(data) < 12:
                            break
                        
                        insn, before, after = struct.unpack('<III', data)
                        
                        # Analyze changes
                        diff = before ^ after
                        
                        msg_parts = []
                        msg_parts.append(f"[Ins: 0x{insn:08x}] 0x{before:08x} -> 0x{after:08x} (Diff: 0x{diff:08x})")
                        
                        # Interpret FPSCR bits (common AArch64/AArch32 VFP)
                        # N Z C V (31-28)
                        flags = []
                        if (diff & (1<<31)): flags.append("N")
                        if (diff & (1<<30)): flags.append("Z")
                        if (diff & (1<<29)): flags.append("C")
                        if (diff & (1<<28)): flags.append("V")
                        if flags:
                            msg_parts.append(f"| Flags: {''.join(flags)}")
                            
                        # Exception Status (0-4 or similar depending on arch, usually IOC, DZC, OFC, UFC, IXC, IDC)
                        exceptions = []
                        if (diff & (1<<0)): exceptions.append("IOC") # Invalid Operation
                        if (diff & (1<<1)): exceptions.append("DZC") # Divide by Zero
                        if (diff & (1<<2)): exceptions.append("OFC") # Overflow
                        if (diff & (1<<3)): exceptions.append("UFC") # Underflow
                        if (diff & (1<<4)): exceptions.append("IXC") # Inexact
                        if (diff & (1<<7)): exceptions.append("IDC") # Input Denormal
                        if exceptions:
                            msg_parts.append(f"| Exceptions: {','.join(exceptions)}")

                        out.write("".join(msg_parts) + "\n")
                        total_changes += 1

            except Exception as e:
                out.write(f"Error reading {filename}: {e}\n")
            
            out.write("\n")
            
        print(f"FPSCR Analysis: {total_changes} total changes.")
        print(f"Detailed FPSCR report: {output_file}")

def write_ranges(filename, ranges):
    # 全局合并
    ranges.sort(key=lambda x: x[0])
    final_ranges = []
    if ranges:
        curr_start, curr_end = ranges[0]
        for start, end in ranges[1:]:
            if start <= curr_end:
                curr_end = max(curr_end, end)
            else:
                final_ranges.append((curr_start, curr_end))
                curr_start, curr_end = start, end
        final_ranges.append((curr_start, curr_end))
        
    dirname = os.path.dirname(filename)
    if dirname:
        os.makedirs(dirname, exist_ok=True)

    with open(filename, 'w') as f:
        for start, end in final_ranges:
            f.write(f"[0x{start:x}, 0x{end:x}]\n")
    return final_ranges

def main():
    # 默认输入目录
    input_dir = "simd_results"
    if len(sys.argv) > 1:
        input_dir = sys.argv[1]
    
    output_simd = "simd_insn_simd.txt"
    output_fpscr = "simd_insn_fpscr.txt"
    output_all = "simd_insn_all.txt"
    
    # 查找所有 .bin 文件
    pattern = os.path.join(input_dir, "res*_complete.bin")
    files = glob.glob(pattern)
    files.sort()
    
    try:
        files.sort(key=lambda x: int(os.path.basename(x).replace('res', '').replace('_complete.bin', '')))
    except:
        pass
    
    if not files:
        print(f"No bin files found in {input_dir}")
        return

    total_simd_count = 0
    all_ranges_simd = []
    
    total_fpscr_count = 0
    all_ranges_fpscr = []
    
    total_all_count = 0
    all_ranges_all = []

    print(f"Found {len(files)} files. Processing...")

    for filepath in files:
        c_simd, r_simd, c_fpscr, r_fpscr, c_all, r_all = process_file(filepath)
        
        total_simd_count += c_simd
        all_ranges_simd.extend(r_simd)
        
        total_fpscr_count += c_fpscr
        all_ranges_fpscr.extend(r_fpscr)
        
        total_all_count += c_all
        all_ranges_all.extend(r_all)

    # 写入文件
    write_ranges(output_simd, all_ranges_simd)
    write_ranges(output_fpscr, all_ranges_fpscr)
    write_ranges(output_all, all_ranges_all)
    
    print(f"\nProcessing complete.")
    print(f"Total instructions affecting SIMD:   {total_simd_count}")
    print(f"Total instructions affecting FPSCR:  {total_fpscr_count}")
    print(f"Total SIMD instructions (Union):     {total_all_count}")
    print(f"\nOutput files:")
    print(f"  SIMD:   {output_simd}")
    print(f"  FPSCR:  {output_fpscr}")
    print(f"  ALL:    {output_all}")

    # Write summary
    summary_file = "simd_insn_summary.txt"
    
    # Process FPSCR logs (saved as res*_cpsr.bin in simd.c)
    fpscr_report_file = "simd_insn_fpscr_details.txt"
    analyze_cpsr_logs(input_dir, fpscr_report_file)

    with open(summary_file, "w") as f:
        f.write(f"Processing complete.\n")
        f.write(f"Total instructions affecting SIMD:   {total_simd_count}\n")
        f.write(f"Total instructions affecting FPSCR:  {total_fpscr_count}\n")
        f.write(f"Total SIMD instructions (Union):     {total_all_count}\n")
        f.write(f"\nOutput files:\n")
        f.write(f"  SIMD:   {output_simd}\n")
        f.write(f"  FPSCR:  {output_fpscr}\n")
        f.write(f"  ALL:    {output_all}\n")
        f.write(f"  FPSCR Details: {fpscr_report_file}\n")
    print(f"  Summary: {summary_file}")

if __name__ == "__main__":
    main()
