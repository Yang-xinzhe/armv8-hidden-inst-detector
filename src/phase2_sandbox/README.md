## Phase 2: Sandbox Analysis

This directory contains **phase2** programs used to further analyze instruction candidates after Phase 1 screening.

The codebase is organized as follows:

- **`demos/`**: Small demos/templates for validating known behaviors (good for sanity checks).
- **`tests/`**: Large-scale analysis programs that iterate candidate encodings and produce binary artifacts.
- **`harness/`**: Assembly entry points for the sandbox.
- **`utils/`**: Helper utilities (e.g., inline ASM validator).

### Workflow & Usage

These programs are typically invoked by the central `dispatcher` (Stage 2) but can be run standalone.

**Via Dispatcher (Recommended):**
```bash
./build/dispatcher -c 4 -x 0 -e ./build/test_arithmetic_a32 -P RK3588 -C A76 -I A32 -S 2
```
The dispatcher automatically:
1. Locates input candidates from **Stage 1** (e.g., `experiments/targets/RK3588/A76/A32/01_screening`).
2. Creates an output directory for **Stage 2** (e.g., `experiments/targets/RK3588/A76/A32/02_fuzzing/arithmetic`).
3. Invokes the worker binary (e.g., `test_arithmetic_a32`) with `-i <input_dir> -o <output_dir> <file_number>`.

**Standalone:**
```bash
./build/test_arithmetic_a32 -i experiments/targets/.../01_screening -o my_results/ 0
```
(Processes `res0_complete.bin` / `candidates_0.bin` from the input directory).

### Analysis Programs (Tests)

These programs live in `tests/` and are built into `build/test_*`.

#### `test_arithmetic` (Target: `build/test_arithmetic_a32`)
- **Goal**: Detect instruction encodings that modify GPRs (R0-R14), CPSR, or SP-like state.
- **Input**: Screening results (candidate bitmaps).
- **Output**:
  - `resN_complete.bin`: Bitmap planes for GPR/CPSR effects.
  - `resN_cpsr.bin`: Logs of CPSR changes.

#### `test_memory` (Target: `build/test_memory_a32`)
- **Goal**: Detect instruction encodings that perform loads/stores using PMU retired events (L1D cache access/refill).
- **Input**: Screening results.
- **Output**: `resN_complete.bin` (Bitmap of memory-active instructions).
- **Requirement**: **User-space PMU access** must be enabled (see `tools/kernel_modules/pmu_user_enable/`).

#### `test_simd` (Target: `build/test_simd_a32`)
- **Goal**: Detect modifications to SIMD/FP registers (Q0-Q31) or FPSCR.
- **Input**: Screening results.
- **Output**: `resN_complete.bin` and FPSCR change logs.

#### `test_control_flow` (Target: `build/test_control_flow_a32`)
- **Goal**: Analyze candidates that caused timeouts in Phase 1 to classify them (loop, deadlock, branch out of sandbox).
- **Input**: Timeout bitmaps from Phase 1.
- **Output**: stdout classification.

### Utilities

#### `inline_asm_validator` (Target: `build/inline_asm_validator`)
A lightweight helper to validate a **single** instruction encoding by embedding it via a preprocessor macro.
```bash
make 0xe1a00001
# Rebuilds build/inline_asm_validator with -DTEST_INSTRUCTION=0xe1a00001
```

### Demos

Located in `demos/`. Useful for verifying the environment.
- `demo_regs.c`: Validates GPR detection logic.
- `demo_pmu.c`: Validates PMU counter access (requires kernel module).
- `demo_simd.c`: Validates SIMD register access.
- `demo_control_flow.c`: Validates signal/timeout handling.

### Related Documentation

- `demos/README.md` (if available): Purpose of each demo.
- `../../tools/kernel_modules/pmu_user_enable/README.md`: Enabling EL0 PMU access.


