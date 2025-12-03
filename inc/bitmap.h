#pragma once
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>


typedef struct {
    uint32_t start;
    uint32_t end;
    uint32_t bits;
    uint32_t size;

    uint8_t *exec_bitmap;
    uint8_t *timeout_bitmap;
} RangeBitmap;

int range_bitmap_init(RangeBitmap *rb, uint32_t start, uint32_t end);
void range_bitmap_mark_exec(RangeBitmap *rb, uint32_t insn);
void range_bitmap_mark_timeout(RangeBitmap *rb, uint32_t insn);
int range_bitmap_has_timeout(const RangeBitmap *rb);
int range_bitmap_flush(const RangeBitmap *rb, FILE *exec_file, FILE *timeout_file);
void range_bitmap_destroy(RangeBitmap *rb);