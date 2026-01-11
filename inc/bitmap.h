#pragma once
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>


/* 可扩展的位图平面，用于记录不同类型的指令属性 */
enum rb_plane_id {
    RB_PLANE_EXEC = 0,
    RB_PLANE_TIMEOUT,
    RB_PLANE_GPR,
    RB_PLANE_CPSR,
    RB_PLANE_LD,
    RB_PLANE_ST,
    RB_PLANE_SP,
    RB_PLANE_SIMD,
    RB_PLANE_FPSCR,
    RB_PLANE_MAX
};

#define HIDR_MAGIC 0x52444948
#define HIDR_VERSION 1

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t count;
    uint32_t reserved;
} RangeFileHeader;

typedef struct {
    uint32_t start;
    uint32_t end;   // Exclusive [start, end)
} RangeEntry;

#define RB_MASK_EXEC    (1u << RB_PLANE_EXEC)
#define RB_MASK_TIMEOUT (1u << RB_PLANE_TIMEOUT)
#define RB_MASK_GPR     (1u << RB_PLANE_GPR)
#define RB_MASK_CPSR    (1u << RB_PLANE_CPSR)
#define RB_MASK_LD      (1u << RB_PLANE_LD)
#define RB_MASK_ST      (1u << RB_PLANE_ST)
#define RB_MASK_SP      (1u << RB_PLANE_SP)
#define RB_MASK_SIMD      (1u << RB_PLANE_SIMD)
#define RB_MASK_FPSCR      (1u << RB_PLANE_FPSCR)

typedef struct {
    uint32_t start;
    uint32_t end;
    uint32_t bits;
    uint32_t size;
    uint32_t plane_mask;               /* 哪些平面已分配 */
    uint8_t *planes[RB_PLANE_MAX];     /* 与 plane_id 对应的位图指针 */
} RangeBitmap;

/* 通用接口 */
int  range_bitmap_init_with_mask(RangeBitmap *rb, uint32_t start, uint32_t end, uint32_t plane_mask);
void range_bitmap_mark(RangeBitmap *rb, enum rb_plane_id plane, uint32_t insn);
int  range_bitmap_plane_has_data(const RangeBitmap *rb, enum rb_plane_id plane);
int  range_bitmap_flush_planes(const RangeBitmap *rb, uint32_t plane_mask, FILE *files[RB_PLANE_MAX]);
int  range_bitmap_write_ranges(const RangeBitmap *rb, enum rb_plane_id plane, FILE *file);
int  range_bitmap_serialize(const RangeBitmap *rb, FILE *file);

/* 辅助 Mark 函数 */
static inline void range_bitmap_mark_gpr(RangeBitmap *rb, uint32_t insn) {
    range_bitmap_mark(rb, RB_PLANE_GPR, insn);
}
static inline void range_bitmap_mark_cpsr(RangeBitmap *rb, uint32_t insn) {
    range_bitmap_mark(rb, RB_PLANE_CPSR, insn);
}
static inline void range_bitmap_mark_ld(RangeBitmap *rb, uint32_t insn) {
    range_bitmap_mark(rb, RB_PLANE_LD, insn);
}
static inline void range_bitmap_mark_st(RangeBitmap *rb, uint32_t insn) {
    range_bitmap_mark(rb, RB_PLANE_ST, insn);
}
static inline void range_bitmap_mark_sp(RangeBitmap *rb, uint32_t insn) {
    range_bitmap_mark(rb, RB_PLANE_SP, insn);
}

/* 兼容旧接口（默认只分配 EXEC / TIMEOUT） */
int  range_bitmap_init(RangeBitmap *rb, uint32_t start, uint32_t end);
void range_bitmap_mark_exec(RangeBitmap *rb, uint32_t insn);
void range_bitmap_mark_timeout(RangeBitmap *rb, uint32_t insn);
int  range_bitmap_has_timeout(const RangeBitmap *rb);
int  range_bitmap_flush(const RangeBitmap *rb, FILE *exec_file, FILE *timeout_file);
int  range_bitmap_flush_exec_timeout(const RangeBitmap *rb, FILE *exec_file, FILE *timeout_file);
void range_bitmap_destroy(RangeBitmap *rb);
