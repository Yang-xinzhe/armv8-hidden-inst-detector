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

def process_file(filepath):
    """
    返回: (count_gpr, ranges_gpr, count_cpsr, ranges_cpsr, count_all, ranges_all)
    """
    # 局部累加
    ranges_gpr = []
    ranges_cpsr = []
    ranges_all = []
    
    cnt_gpr = 0
    cnt_cpsr = 0
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
                
                # 解析位图
                indices_gpr_local = set()
                indices_cpsr_local = set()
                indices_all_local = set()
                
                for i in range(num_bits):
                    byte_idx = i // 8
                    bit_idx = i % 8
                    
                    is_gpr = (gpr_bitmap[byte_idx] >> bit_idx) & 1
                    is_cpsr = (cpsr_bitmap[byte_idx] >> bit_idx) & 1
                    
                    if is_gpr:
                        indices_gpr_local.add(i)
                    if is_cpsr:
                        indices_cpsr_local.add(i)
                    if is_gpr or is_cpsr:
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

    return cnt_gpr, ranges_gpr, cnt_cpsr, ranges_cpsr, cnt_all, ranges_all

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
    output_all = "arithmetic_insn_all.txt"
    
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

    total_gpr_count = 0
    all_ranges_gpr = []
    
    total_cpsr_count = 0
    all_ranges_cpsr = []
    
    total_all_count = 0
    all_ranges_all = []

    print(f"Found {len(files)} files. Processing...")

    for filepath in files:
        c_gpr, r_gpr, c_cpsr, r_cpsr, c_all, r_all = process_file(filepath)
        
        total_gpr_count += c_gpr
        all_ranges_gpr.extend(r_gpr)
        
        total_cpsr_count += c_cpsr
        all_ranges_cpsr.extend(r_cpsr)
        
        total_all_count += c_all
        all_ranges_all.extend(r_all)

    # 写入文件
    write_ranges(output_gpr, all_ranges_gpr)
    write_ranges(output_cpsr, all_ranges_cpsr)
    write_ranges(output_all, all_ranges_all)
    
    print(f"\nProcessing complete.")
    print(f"Total instructions affecting GPR:  {total_gpr_count}")
    print(f"Total instructions affecting CPSR: {total_cpsr_count}")
    print(f"Total Arithmetic instructions (Union): {total_all_count}")
    print(f"\nOutput files:")
    print(f"  GPR:  {output_gpr}")
    print(f"  CPSR: {output_cpsr}")
    print(f"  ALL:  {output_all}")

    # Write summary
    summary_file = "arithmetic_insn_summary.txt"
    with open(summary_file, "w") as f:
        f.write(f"Processing complete.\n")
        f.write(f"Total instructions affecting GPR:  {total_gpr_count}\n")
        f.write(f"Total instructions affecting CPSR: {total_cpsr_count}\n")
        f.write(f"Total Arithmetic instructions (Union): {total_all_count}\n")
        f.write(f"\nOutput files:\n")
        f.write(f"  GPR:  {output_gpr}\n")
        f.write(f"  CPSR: {output_cpsr}\n")
        f.write(f"  ALL:  {output_all}\n")
    print(f"  Summary: {summary_file}")

if __name__ == "__main__":
    main()