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
    返回: (count_ld, ranges_ld, count_st, ranges_st, count_all, ranges_all)
    """
    ranges_ld = []
    ranges_st = []
    ranges_all = []
    
    cnt_ld = 0
    cnt_st = 0
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
            
            for idx in range(range_count):
                # C 代码实际上写入了 Start(4), End(4), Size(4) -> 共 12 字节
                range_header = f.read(12)
                if len(range_header) < 12:
                    break
                
                start, end, size = struct.unpack('<III', range_header)
                
                if idx < 5: 
                    print(f"  Range {idx}: start=0x{start:x}, end=0x{end:x}, size={size}")

                # 读取 LD Bitmap (Block 1)
                block1 = f.read(size)
                if len(block1) != size:
                    print(f"Warning: Unexpected EOF reading LD block in {filepath}")
                    break
                
                # 读取 ST Bitmap (Block 2)
                # 根据 C 代码逻辑，如果有两个 plane，它们是连续存储的
                block2 = f.read(size)
                if len(block2) != size:
                     # 也许只有 1 个 plane?
                     # 如果读不到 Block 2，那只能说明这个文件是单 Plane 的。
                     # 我们把 Block 2 设为 None，只处理 LD。
                     block2 = None
                     # 注意：如果这真的是单 Plane，那么刚才读失败的 bytes 必须吐回去吗？
                     # 不，如果它没读够 size 长度，说明文件结束了，或者文件损坏。
                     if len(block2) > 0:
                         print(f"Warning: Incomplete ST block in {filepath} (got {len(block2)}/{size})")

                indices_ld_local = set()
                indices_st_local = set()
                indices_all_local = set()

                num_bits = end - start

                # 解析 LD Bitmap (Block 1)
                for i in range(num_bits):
                    byte_idx = i // 8
                    bit_idx = i % 8
                    is_set = (block1[byte_idx] >> bit_idx) & 1
                    if is_set:
                        indices_ld_local.add(i)
                        indices_all_local.add(i)

                # 解析 ST Bitmap (Block 2)
                if block2:
                    for i in range(num_bits):
                        byte_idx = i // 8
                        bit_idx = i % 8
                        is_set = (block2[byte_idx] >> bit_idx) & 1
                        if is_set:
                            indices_st_local.add(i)
                            indices_all_local.add(i)
                
                # 处理 LD
                if indices_ld_local:
                    abs_ld = sorted([start + i for i in indices_ld_local])
                    cnt_ld += len(abs_ld)
                    ranges_ld.extend(merge_ranges(abs_ld))

                # 处理 ST
                if indices_st_local:
                    abs_st = sorted([start + i for i in indices_st_local])
                    cnt_st += len(abs_st)
                    ranges_st.extend(merge_ranges(abs_st))

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

    return cnt_ld, ranges_ld, cnt_st, ranges_st, cnt_all, ranges_all

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
    input_dir = "memaccess_results"
    if len(sys.argv) > 1:
        input_dir = sys.argv[1]
    
    output_ld = "memaccess_insn_ld.txt"
    output_st = "memaccess_insn_st.txt"
    output_all = "memaccess_insn_all.txt"
    
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

    total_ld_count = 0
    all_ranges_ld = []
    
    total_st_count = 0
    all_ranges_st = []
    
    total_all_count = 0
    all_ranges_all = []

    print(f"Found {len(files)} files. Processing...")

    for filepath in files:
        c_ld, r_ld, c_st, r_st, c_all, r_all = process_file(filepath)
        
        total_ld_count += c_ld
        all_ranges_ld.extend(r_ld)
        
        total_st_count += c_st
        all_ranges_st.extend(r_st)
        
        total_all_count += c_all
        all_ranges_all.extend(r_all)

    # 写入文件
    write_ranges(output_ld, all_ranges_ld)
    write_ranges(output_st, all_ranges_st)
    write_ranges(output_all, all_ranges_all)
    
    print(f"\nProcessing complete.")
    print(f"Total instructions affecting LD:  {total_ld_count}")
    print(f"Total instructions affecting ST:  {total_st_count}")
    print(f"Total MemAccess instructions (Union): {total_all_count}")
    print(f"\nOutput files:")
    print(f"  LD:   {output_ld}")
    print(f"  ST:   {output_st}")
    print(f"  ALL:  {output_all}")

    # Write summary
    summary_file = "memaccess_insn_summary.txt"
    with open(summary_file, "w") as f:
        f.write(f"Processing complete.\n")
        f.write(f"Total instructions affecting LD:  {total_ld_count}\n")
        f.write(f"Total instructions affecting ST:  {total_st_count}\n")
        f.write(f"Total MemAccess instructions (Union): {total_all_count}\n")
        f.write(f"\nOutput files:\n")
        f.write(f"  LD:   {output_ld}\n")
        f.write(f"  ST:   {output_st}\n")
        f.write(f"  ALL:  {output_all}\n")
    print(f"  Summary: {summary_file}")

if __name__ == "__main__":
    main()

