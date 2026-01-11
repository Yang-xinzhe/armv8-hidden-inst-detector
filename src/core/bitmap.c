#include "bitmap.h"
#include <string.h>

static int bitmap_has_data(const uint8_t *bitmap, uint32_t size)
{
    if (!bitmap || size == 0) {
        return 0;
    }

    for (uint32_t i = 0; i < size; i++) {
        if (bitmap[i] != 0) {
            return 1;
        }
    }
    return 0;
}

static void bitmap_set_bit(uint8_t *bitmap,
                           uint32_t bits,
                           uint32_t start,
                           uint32_t insn)
{
    if (!bitmap) return;

    if (insn < start) return;
    uint32_t offset = insn - start;

    if (offset >= bits) {
        return;
    }

    uint32_t byte_index  = offset / 8;
    uint8_t  bit_position = offset % 8;

    bitmap[byte_index] |= (uint8_t)(1u << bit_position);
}

int range_bitmap_init_with_mask(RangeBitmap *rb, uint32_t start, uint32_t end, uint32_t plane_mask)
{
    if (!rb) return -1;
    if (end <= start) {
        return -1;
    }

    uint32_t bits = end - start;
    uint32_t size = (bits + 7u) / 8u;

    memset(rb, 0, sizeof(*rb));
    rb->start = start;
    rb->end   = end;
    rb->bits  = bits;
    rb->size  = size;
    rb->plane_mask = plane_mask;

    for (int p = 0; p < RB_PLANE_MAX; ++p) {
        if (plane_mask & (1u << p)) {
            rb->planes[p] = (uint8_t *)calloc(size, 1);
            if (!rb->planes[p]) {
                perror("calloc bitmap plane failed");
                range_bitmap_destroy(rb);
                return -1;
            }
        }
    }

    return 0;
}

int range_bitmap_init(RangeBitmap *rb, uint32_t start, uint32_t end)
{
    return range_bitmap_init_with_mask(rb, start, end, RB_MASK_EXEC | RB_MASK_TIMEOUT);
}

void range_bitmap_mark(RangeBitmap *rb, enum rb_plane_id plane, uint32_t insn)
{
    if (!rb) return;
    uint32_t mask = (1u << plane);
    if (!(rb->plane_mask & mask)) return;
    bitmap_set_bit(rb->planes[plane], rb->bits, rb->start, insn);
}

void range_bitmap_mark_exec(RangeBitmap *rb, uint32_t insn)
{
    range_bitmap_mark(rb, RB_PLANE_EXEC, insn);
}

void range_bitmap_mark_timeout(RangeBitmap *rb, uint32_t insn)
{
    range_bitmap_mark(rb, RB_PLANE_TIMEOUT, insn);
}

int range_bitmap_plane_has_data(const RangeBitmap *rb, enum rb_plane_id plane)
{
    if (!rb) return 0;
    if (!(rb->plane_mask & (1u << plane))) return 0;
    return bitmap_has_data(rb->planes[plane], rb->size);
}

int range_bitmap_has_timeout(const RangeBitmap *rb)
{
    return range_bitmap_plane_has_data(rb, RB_PLANE_TIMEOUT);
}

int range_bitmap_flush_exec_timeout(const RangeBitmap *rb,
                                    FILE *exec_file,
                                    FILE *timeout_file)
{
    if (!rb || !exec_file) {
        return -1;
    }
    if (!(rb->plane_mask & RB_MASK_EXEC) || !rb->planes[RB_PLANE_EXEC]) {
        return -1;
    }

    // 写 exec（complete）文件：永远写
    if (fwrite(&rb->start, sizeof(uint32_t), 1, exec_file) != 1) {
        return -1;
    }
    if (fwrite(&rb->end,   sizeof(uint32_t), 1, exec_file) != 1) {
        return -1;
    }
    if (fwrite(&rb->size,  sizeof(uint32_t), 1, exec_file) != 1) {
        return -1;
    }
    if (fwrite(rb->planes[RB_PLANE_EXEC], 1, rb->size, exec_file) != rb->size) {
        return -1;
    }

    int timeout_written = 0;

    // timeout 文件：只有有数据时才写
    if (timeout_file &&
        (rb->plane_mask & RB_MASK_TIMEOUT) &&
        rb->planes[RB_PLANE_TIMEOUT] &&
        bitmap_has_data(rb->planes[RB_PLANE_TIMEOUT], rb->size))
    {
        if (fwrite(&rb->start, sizeof(uint32_t), 1, timeout_file) != 1) {
            return -1;
        }
        if (fwrite(&rb->end,   sizeof(uint32_t), 1, timeout_file) != 1) {
            return -1;
        }
        if (fwrite(&rb->size,  sizeof(uint32_t), 1, timeout_file) != 1) {
            return -1;
        }
        if (fwrite(rb->planes[RB_PLANE_TIMEOUT], 1, rb->size, timeout_file) != rb->size) {
            return -1;
        }

        timeout_written = 1;
    }

    return timeout_written;
}

int range_bitmap_flush(const RangeBitmap *rb,
                       FILE *exec_file,
                       FILE *timeout_file)
{
    return range_bitmap_flush_exec_timeout(rb, exec_file, timeout_file);
}

int range_bitmap_flush_planes(const RangeBitmap *rb,
                              uint32_t plane_mask,
                              FILE *files[RB_PLANE_MAX])
{
    if (!rb) return -1;

    for (int p = 0; p < RB_PLANE_MAX; ++p) {
        uint32_t mask = (1u << p);
        if (!(plane_mask & mask)) continue;
        if (!(rb->plane_mask & mask)) continue;
        if (!files || !files[p]) continue;

        uint8_t *plane = rb->planes[p];
        if (!plane) continue;

        /* 默认只有有数据才写，避免产生空块 */
        if (!bitmap_has_data(plane, rb->size)) continue;

        if (fwrite(&rb->start, sizeof(uint32_t), 1, files[p]) != 1) return -1;
        if (fwrite(&rb->end,   sizeof(uint32_t), 1, files[p]) != 1) return -1;
        if (fwrite(&rb->size,  sizeof(uint32_t), 1, files[p]) != 1) return -1;
        if (fwrite(plane, 1, rb->size, files[p]) != rb->size) return -1;
    }

    return 0;
}

int range_bitmap_serialize(const RangeBitmap *rb, FILE *file)
{
    if (!rb || !file) return -1;
    
    // Write header once
    if (fwrite(&rb->start, sizeof(uint32_t), 1, file) != 1) return -1;
    if (fwrite(&rb->end,   sizeof(uint32_t), 1, file) != 1) return -1;
    if (fwrite(&rb->size,  sizeof(uint32_t), 1, file) != 1) return -1;

    // Write all allocated planes in order
    for (int p = 0; p < RB_PLANE_MAX; ++p) {
        if (rb->plane_mask & (1u << p)) {
             if (rb->planes[p]) {
                 if (fwrite(rb->planes[p], 1, rb->size, file) != rb->size) return -1;
             } else {
                 // Should not happen if initialized correctly, but as fallback write zeros? 
                 // Or error out. Let's return error to be safe.
                 return -1;
             }
        }
    }
    return 0;
}

int range_bitmap_write_ranges(const RangeBitmap *rb, enum rb_plane_id plane, FILE *file)
{
    if (!rb || !file) return -1;
    if (plane >= RB_PLANE_MAX || !(rb->plane_mask & (1u << plane))) return 0;
    
    uint8_t *bitmap = rb->planes[plane];
    // If bitmap is NULL but mask is set, treat as empty (0 ranges)
    if (!bitmap) return 0;

    uint32_t bits = rb->bits;
    uint32_t start_insn = rb->start;
    int range_count = 0;
    
    int in_range = 0;
    uint32_t current_start = 0;
    
    for (uint32_t i = 0; i < bits; ++i) {
        int is_set = (bitmap[i / 8] >> (i % 8)) & 1;
        
        if (is_set) {
            if (!in_range) {
                in_range = 1;
                current_start = start_insn + i;
            }
        } else {
            if (in_range) {
                // Range End (Exclusive)
                uint32_t current_end = start_insn + i;
                if (fwrite(&current_start, sizeof(uint32_t), 1, file) != 1) return -1;
                if (fwrite(&current_end, sizeof(uint32_t), 1, file) != 1) return -1;
                range_count++;
                in_range = 0;
            }
        }
    }
    
    // Handle last range
    if (in_range) {
        uint32_t current_end = start_insn + bits;
        if (fwrite(&current_start, sizeof(uint32_t), 1, file) != 1) return -1;
        if (fwrite(&current_end, sizeof(uint32_t), 1, file) != 1) return -1;
        range_count++;
    }

    return range_count;
}

void range_bitmap_destroy(RangeBitmap *rb)
{
    if (!rb) return;

    for (int p = 0; p < RB_PLANE_MAX; ++p) {
        if (rb->planes[p]) {
            free(rb->planes[p]);
            rb->planes[p] = NULL;
        }
    }

    rb->start = rb->end = rb->bits = rb->size = rb->plane_mask = 0;
}
