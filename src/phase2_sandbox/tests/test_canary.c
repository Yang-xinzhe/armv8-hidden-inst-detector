#include "core.h"
#include "register_states.h"
#include "bitmap.h"
#include "sandbox.h"
#include "test_runner.h"

static void* canary_create_bitmap(uint32_t start, uint32_t end, const char *out_dir, int file_number)
{
    char log_path[1024];
    snprintf(log_path, sizeof(log_path), "%s/res%d_canary.txt", out_dir, file_number);
    (void)start;(void)end;
    FILE *f = fopen(log_path, "w");
    if (!f) {
        perror("fopen canary log");
        return NULL;
    }
    return f;
}

static void canary_run_insn(uint32_t insn, void *bitmap_ptr)
{
    FILE *log_file = (FILE *)bitmap_ptr;
    if (!log_file) return;

    uint8_t insn_bytes[4];
    size_t buf_length = fill_insn_buffer(insn_bytes, sizeof(insn_bytes), insn);
    
    execute_insn_page_screen(insn_bytes, buf_length);
    
    if (last_insn_signum == 0) {
        // 记录导致提权的指令
        fprintf(log_file, "0x%08x\n", insn);
        fflush(log_file);
    }
}

static int canary_flush_bitmap(void *bitmap_ptr, FILE *file)
{
    (void)file; 
    FILE *log_file = (FILE *)bitmap_ptr;
    if (log_file) fflush(log_file);
    return 0;
}

static void canary_destroy_bitmap(void *bitmap_ptr)
{
    FILE *f = (FILE *)bitmap_ptr;
    if (f) {
        fclose(f);
    }
}

static const TestOps canary_ops = {
    .test_name = "canary_priv_escalation",
    .global_init = NULL,
    .create_bitmap = canary_create_bitmap,
    .run_insn = canary_run_insn,
    .flush_bitmap = canary_flush_bitmap,
    .destroy_bitmap = canary_destroy_bitmap
};

int main(int argc, char *argv[]) {
    return run_test_framework(argc, argv, &canary_ops);
}