#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#include <stdint.h>
#include <stdio.h>
#include "core.h"
#include "sandbox.h"
#include "bitmap.h"

// ------------------------------------------------------------------
// Test Operations Interface
// ------------------------------------------------------------------

/**
 * @brief 回调接口，由具体的测试用例（如 Arithmetic, Memory）实现
 */
typedef struct {
    // 测试名称，用于生成输出目录名 (e.g. "arithmetic_results")
    const char *test_name;

    // [Optional] 全局初始化（只调用一次）
    // 返回 0 成功，非 0 失败
    int (*global_init)(void);

    // [Required] 为当前 Range 创建 Bitmap 实例
    // out_dir: 输出目录路径 (用于创建额外的日志文件)
    // file_number: 当前处理的文件编号 (用于生成日志文件名)
    // 返回 Bitmap 指针（void*），如果失败返回 NULL
    void* (*create_bitmap)(uint32_t start, uint32_t end, const char *out_dir, int file_number);

    // [Required] 执行单条指令测试
    // insn: 待测指令
    // bitmap: 当前 Range 的 Bitmap 指针
    void (*run_insn)(uint32_t insn, void *bitmap);

    // [Required] 将 Bitmap 刷入文件
    // 返回 0 成功
    int (*flush_bitmap)(void *bitmap, FILE *file);

    // [Required] 销毁 Bitmap
    void (*destroy_bitmap)(void *bitmap);

} TestOps;

// ------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------

/**
 * @brief 运行测试框架的主入口
 * 
 * @param argc main函数的 argc
 * @param argv main函数的 argv
 * @param ops  具体的测试逻辑回调
 * @return int exit code
 */
int run_test_framework(int argc, char *argv[], const TestOps *ops);

#endif // TEST_RUNNER_H
