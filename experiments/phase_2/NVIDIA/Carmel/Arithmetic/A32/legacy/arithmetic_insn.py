import os
import struct
import sys
import glob

def parse_bitmap(data, size_bytes, num_bits):
    """
    解析位图数据，返回置位的索引列表（相对偏移）
    """
    indices = []
    for i in range(num_bits):
        byte_index = i // 8
        bit_index = i % 8
        if byte_index < size_bytes:
            if (data[byte_index] >> bit_index) & 1:
                indices.append(i)
    return indices

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

def format_cpsr_diff(diff):
    changes = []
    # AArch32 CPSR bits
    if diff & (1 << 31): changes.append("N")
    if diff & (1 << 30): changes.append("Z")
    if diff & (1 << 29): changes.append("C")
    if diff & (1 << 28): changes.append("V")
    if diff & (1 << 27): changes.append("Q")
    if diff & (1 << 9):  changes.append("E") # Endianness
    if diff & (1 << 8):  changes.append("A") # Async abort mask
    if diff & (1 << 7):  changes.append("I") # IRQ mask
    if diff & (1 << 6):  changes.append("F") # FIQ mask
    if diff & (1 << 5):  changes.append("T") # Thumb
    if diff & 0x1F:      changes.append(f"Mode(0x{diff&0x1F:x})")
    
    if not changes:
        return f"Unknown(0x{diff:x})"
    return ",".join(changes)

def process_cpsr_log_stream(filepath, output_file_handle):
    """
    流式解析对应的 _cpsr.bin 文件，写入详情，并统计修改类型
    返回: (count_flags_changed, count_mode_changed)
    """
    cnt_flags = 0
    cnt_mode = 0
    
    if not os.path.exists(filepath):
        return 0, 0
        
    try:
        with open(filepath, 'rb') as f:
            while True:
                data = f.read(12) # 3 * 4 bytes
                if len(data) < 12:
                    break
                insn, before, after = struct.unpack('<III', data)
                diff = before ^ after
                
                # 统计逻辑
                # Flags: N(31), Z(30), C(29), V(28), Q(27)
                if diff & 0xF8000000:
                    cnt_flags += 1
                
                # Mode: M[4:0] (Bits 0-4)
                if diff & 0x1F:
                    cnt_mode += 1
                
                changes_str = format_cpsr_diff(diff)
                output_file_handle.write(f"0x{insn:<10x} 0x{before:<10x} 0x{after:<10x} 0x{diff:<10x} {changes_str}\n")
                
    except Exception as e:
        print(f"Error processing CPSR log {filepath}: {e}")
        
    return cnt_flags, cnt_mode

def process_file(filepath):
    """
    返回: (count_gpr, ranges_gpr, count_cpsr, ranges_cpsr, count_sp, ranges_sp, count_all, ranges_all)
    """
    ranges_gpr = []
    ranges_cpsr = []
    ranges_sp = []
    ranges_all = []
    
    cnt_gpr = 0
    cnt_cpsr = 0
    cnt_sp = 0
    cnt_all = 0
    
    try:
        filesize = os.path.getsize(filepath)
        if filesize == 0:
            return 0, [], 0, [], 0, [], 0, []

        with open(filepath, 'rb') as f:
            # 读取文件头: file_number (4 bytes), range_count (4 bytes)
            header_data = f.read(8)
            if len(header_data) < 8:
                print(f"Warning: File {filepath} is too short for header")
                return 0, [], 0, [], 0, [], 0, []
            
            file_num, range_count = struct.unpack('<ii', header_data)
            
            for _ in range(range_count):
                # 读取 Range Header: start(4), end(4), size(4)
                range_header = f.read(12)
                if len(range_header) < 12:
                    break
                
                start, end, size = struct.unpack('<III', range_header)
                num_bits = end - start
                
                # 读取 GPR bitmap
                gpr_bitmap = f.read(size)
                if len(gpr_bitmap) != size:
                    print(f"Warning: Unexpected EOF reading GPR bitmap in {filepath}")
                    break
                    
                # 读取 CPSR bitmap
                cpsr_bitmap = f.read(size)
                if len(cpsr_bitmap) != size:
                    print(f"Warning: Unexpected EOF reading CPSR bitmap in {filepath}")
                    break
                
                # 读取 SP bitmap
                sp_bitmap = f.read(size)
                if len(sp_bitmap) != size:
                    print(f"Warning: Unexpected EOF reading SP bitmap in {filepath}")
                    break
                
                # 解析位图
                indices_gpr_local = set()
                indices_cpsr_local = set()
                indices_sp_local = set()
                indices_all_local = set()
                
                for i in range(num_bits):
                    byte_idx = i // 8
                    bit_idx = i % 8
                    
                    is_gpr = (gpr_bitmap[byte_idx] >> bit_idx) & 1
                    is_cpsr = (cpsr_bitmap[byte_idx] >> bit_idx) & 1
                    is_sp = (sp_bitmap[byte_idx] >> bit_idx) & 1
                    
                    if is_gpr: indices_gpr_local.add(i)
                    if is_cpsr: indices_cpsr_local.add(i)
                    if is_sp: indices_sp_local.add(i)
                    
                    if is_gpr or is_cpsr or is_sp:
                        indices_all_local.add(i)
                
                # 处理 GPR
                if indices_gpr_local:
                    abs_gpr = sorted([start + i for i in indices_gpr_local])
                    cnt_gpr += len(abs_gpr)
                    ranges_gpr.extend(merge_ranges(abs_gpr))

                # 处理 CPSR
                if indices_cpsr_local:
                    abs_cpsr = sorted([start + i for i in indices_cpsr_local])
                    cnt_cpsr += len(abs_cpsr)
                    ranges_cpsr.extend(merge_ranges(abs_cpsr))
                    
                # 处理 SP
                if indices_sp_local:
                    abs_sp = sorted([start + i for i in indices_sp_local])
                    cnt_sp += len(abs_sp)
                    ranges_sp.extend(merge_ranges(abs_sp))

                # 处理 Union (All)
                if indices_all_local:
                    abs_all = sorted([start + i for i in indices_all_local])
                    cnt_all += len(abs_all)
                    ranges_all.extend(merge_ranges(abs_all))
                    
    except Exception as e:
        print(f"Error processing {filepath}: {e}")
        import traceback
        traceback.print_exc()
        return 0, [], 0, [], 0, [], 0, []

    return cnt_gpr, ranges_gpr, cnt_cpsr, ranges_cpsr, cnt_sp, ranges_sp, cnt_all, ranges_all

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
    input_dir = "arithmetic_results"
    if len(sys.argv) > 1:
        input_dir = sys.argv[1]
    
    output_gpr = "arithmetic_insn_gpr.txt"
    output_cpsr = "arithmetic_insn_cpsr.txt"
    output_sp = "arithmetic_insn_sp.txt"
    output_all = "arithmetic_insn_all.txt"
    output_cpsr_details = "arithmetic_insn_cpsr_details.txt"
    
    # 查找所有 .bin 文件
    pattern = os.path.join(input_dir, "res*_complete.bin")
    files = glob.glob(pattern)
    
    try:
        files.sort(key=lambda x: int(os.path.basename(x).replace('res', '').replace('_complete.bin', '')))
    except:
        files.sort()
    
    if not files:
        print(f"No bin files found in {input_dir}")
        return

    total_gpr_count = 0
    all_ranges_gpr = []
    
    total_cpsr_count = 0
    all_ranges_cpsr = []
    
    total_sp_count = 0
    all_ranges_sp = []
    
    total_all_count = 0
    all_ranges_all = []

    # 新增统计
    total_cpsr_flags_changed = 0
    total_cpsr_mode_changed = 0

    print(f"Found {len(files)} files. Processing...")

    # 打开 CPSR 详情文件进行流式写入
    with open(output_cpsr_details, 'w') as f_cpsr_out:
        f_cpsr_out.write(f"{'Insn':<12} {'Before':<12} {'After':<12} {'Diff':<12} {'Changes'}\n")
        f_cpsr_out.write("-" * 65 + "\n")
        
        for filepath in files:
            # 1. 解析位图 (Basic counts and ranges)
            c_gpr, r_gpr, c_cpsr, r_cpsr, c_sp, r_sp, c_all, r_all = process_file(filepath)
            
            total_gpr_count += c_gpr
            all_ranges_gpr.extend(r_gpr)
            
            total_cpsr_count += c_cpsr
            all_ranges_cpsr.extend(r_cpsr)
            
            total_sp_count += c_sp
            all_ranges_sp.extend(r_sp)
            
            total_all_count += c_all
            all_ranges_all.extend(r_all)

            # 2. 解析 CPSR 详细日志 (流式处理 + 统计 Flags/Mode)
            cpsr_log_path = filepath.replace("_complete.bin", "_cpsr.bin")
            cnt_flags, cnt_mode = process_cpsr_log_stream(cpsr_log_path, f_cpsr_out)
            
            total_cpsr_flags_changed += cnt_flags
            total_cpsr_mode_changed += cnt_mode

    # 写入范围文件
    write_ranges(output_gpr, all_ranges_gpr)
    write_ranges(output_cpsr, all_ranges_cpsr)
    write_ranges(output_sp, all_ranges_sp)
    write_ranges(output_all, all_ranges_all)
    
    print(f"\nProcessing complete.")
    print(f"Total instructions affecting GPR:   {total_gpr_count}")
    print(f"Total instructions affecting SP:    {total_sp_count}")
    print(f"Total instructions affecting CPSR:  {total_cpsr_count}")
    print(f"  - Affecting Flags (NZCVQ):      {total_cpsr_flags_changed}")
    print(f"  - Affecting Mode (M[4:0]):      {total_cpsr_mode_changed} (DANGEROUS!)")
    print(f"Total Arithmetic instructions (Union): {total_all_count}")
    print(f"\nOutput files:")
    print(f"  GPR:          {output_gpr}")
    print(f"  CPSR:         {output_cpsr}")
    print(f"  SP:           {output_sp}")
    print(f"  ALL:          {output_all}")
    print(f"  CPSR Details: {output_cpsr_details}")

    # Write summary
    summary_file = "arithmetic_insn_summary.txt"
    with open(summary_file, "w") as f:
        f.write(f"Processing complete.\n")
        f.write(f"Total instructions affecting GPR:   {total_gpr_count}\n")
        f.write(f"Total instructions affecting SP:    {total_sp_count}\n")
        f.write(f"Total instructions affecting CPSR:  {total_cpsr_count}\n")
        f.write(f"  - Affecting Flags (NZCVQ):      {total_cpsr_flags_changed}\n")
        f.write(f"  - Affecting Mode (M[4:0]):      {total_cpsr_mode_changed}\n")
        f.write(f"Total Arithmetic instructions (Union): {total_all_count}\n")
        f.write(f"\nOutput files:\n")
        f.write(f"  GPR:          {output_gpr}\n")
        f.write(f"  CPSR:         {output_cpsr}\n")
        f.write(f"  SP:           {output_sp}\n")
        f.write(f"  ALL:          {output_all}\n")
        f.write(f"  CPSR Details: {output_cpsr_details}\n")
    print(f"  Summary: {summary_file}")

if __name__ == "__main__":
    main()