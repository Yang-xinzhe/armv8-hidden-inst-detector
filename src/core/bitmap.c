#include "bitmap.h"

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


int range_bitmap_init(RangeBitmap *rb, uint32_t start, uint32_t end)
{
    if (!rb) return -1;
    if (end <= start) {
        return -1;
    }

    uint32_t bits = end - start;
    uint32_t size = (bits + 7u) / 8u;

    uint8_t *exec = (uint8_t *)calloc(size, 1);
    if (!exec) {
        perror("calloc exec_bitmap failed");
        return -1;
    }

    uint8_t *timeout = (uint8_t *)calloc(size, 1);
    if (!timeout) {
        perror("calloc timeout_bitmap failed");
        free(exec);
        return -1;
    }

    rb->start          = start;
    rb->end            = end;
    rb->bits           = bits;
    rb->size           = size;
    rb->exec_bitmap    = exec;
    rb->timeout_bitmap = timeout;

    return 0;
}

void range_bitmap_mark_exec(RangeBitmap *rb, uint32_t insn)
{
    if (!rb || !rb->exec_bitmap) return;
    bitmap_set_bit(rb->exec_bitmap, rb->bits, rb->start, insn);
}

void range_bitmap_mark_timeout(RangeBitmap *rb, uint32_t insn)
{
    if (!rb || !rb->timeout_bitmap) return;
    bitmap_set_bit(rb->timeout_bitmap, rb->bits, rb->start, insn);
}

int range_bitmap_has_timeout(const RangeBitmap *rb)
{
    if (!rb || !rb->timeout_bitmap) return 0;
    return bitmap_has_data(rb->timeout_bitmap, rb->size);
}

int range_bitmap_flush(const RangeBitmap *rb,
                       FILE *exec_file,
                       FILE *timeout_file)
{
    if (!rb || !exec_file || !rb->exec_bitmap) {
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
    if (fwrite(rb->exec_bitmap, 1, rb->size, exec_file) != rb->size) {
        return -1;
    }

    int timeout_written = 0;

    // timeout 文件：只有有数据时才写
    if (timeout_file && rb->timeout_bitmap &&
        bitmap_has_data(rb->timeout_bitmap, rb->size))
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
        if (fwrite(rb->timeout_bitmap, 1, rb->size, timeout_file) != rb->size) {
            return -1;
        }

        timeout_written = 1;
    }

    return timeout_written;
}

void range_bitmap_destroy(RangeBitmap *rb)
{
    if (!rb) return;

    if (rb->exec_bitmap) {
        free(rb->exec_bitmap);
        rb->exec_bitmap = NULL;
    }

    if (rb->timeout_bitmap) {
        free(rb->timeout_bitmap);
        rb->timeout_bitmap = NULL;
    }

    rb->start = rb->end = rb->bits = rb->size = 0;
}


