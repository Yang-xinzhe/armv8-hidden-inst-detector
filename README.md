## ARMv8 Hidden Instruction Detector (AArch32 sandbox)

This repository contains the experiment code used in my paper to **screen** and **analyze** potentially hidden/undefined ARM instructions by executing candidate encodings inside a fault-tolerant sandbox (signal/timeout guarded).

The codebase is organized into two phases:

- **Phase 1 (screening)**: large-scale execution of instruction ranges and producing bitmaps indicating which encodings execute / timeout.
- **Phase 2 (sandbox analysis)**: deeper analysis on selected candidates (e.g., register side-effects, memory access behavior, SIMD/FPSCR effects, control-flow anomalies), plus several small demos/templates.

### Repository layout

- **`inc/`**: public headers used across the project.
- **`src/core/`**: core runtime components:
  - sandbox execution (`mmap` RWX page, signal handling, watchdog timer)
  - CPU pinning helpers
  - bitmap utilities and PMU helpers
- **`src/phase1_screening/`**:
  - `dispatcher_screen.c`: schedules jobs across cores (fork + CPU affinity)
  - `worker_screen.c`: executes ranges and writes bitmap results
- **`src/phase2_sandbox/`**:
  - analysis programs for large-scale validation/characterization (e.g., `fuzzer_arithmetic.c`, `fuzzer_memaccess.c`, `fuzzer_simd.c`, `fuzzer_control_flow.c`)
  - `sandbox_demos/`: small demos/templates used to validate known behaviors
  - `pmu_user_module/`: kernel module used to enable user-space PMU access (requires `insmod`)
- **`res/`**: local experiment outputs (ignored by git by default; keep your results here).

### Build

The default toolchain is configured for **AArch32** execution on ARMv8:

- Compiler: `arm-linux-gnueabihf-gcc` (see `CC` in `Makefile`)

Build everything:

```bash
make
```

Outputs are placed under `build/` (e.g., `build/dispatcher`, `build/worker`, ...).
Demo binaries are placed under `build/demo/`.

### Phase 1: screening (dispatcher + worker)

The dispatcher expects an input directory containing files named `res%d.txt` (e.g., `res0.txt`, `res1.txt`, ...). Each file contains ranges per line:

```
[start, end]
```

In phase1, the current parser expects **decimal** numbers (C `sscanf(..., "[%u, %u]")`).

Example:

```bash
mkdir -p results_A32 bitmap_results

./build/dispatcher \
  -c 4 \
  -o 0 \
  -e ./build/worker \
  -d results_A32 \
  -r bitmap_results
```

Expected outputs (per input file number `N`) under the output directory (default: `bitmap_results/`):

- `resN_complete.bin`
- `resN_timeout.bin`

### Phase 2: sandbox analysis & demos

Phase2 programs are located in `src/phase2_sandbox/`. Many of them read inputs from a directory named `hidden_insn/` by default (e.g., `hidden_insn/resN.txt`, `hidden_insn/resN_timeout_decoded.txt`). See:

- `src/phase2_sandbox/README.md`
- `src/phase2_sandbox/sandbox_demos/README.md`
- `src/phase2_sandbox/pmu_user_module/README.md`

### Notes on results (`res/`)

The `res/` directory is intended for **local experiment artifacts** and is ignored by git (except a `.gitkeep` placeholder). This keeps the repository clean while allowing you to store large and messy intermediate outputs locally.

### Troubleshooting

- **“Permission denied” / PMU access issues**: you may need to build and insert the kernel module under `src/phase2_sandbox/pmu_user_module/` and run with proper privileges.
- **Directory not found / hard-coded paths**: use `config/project.conf` (and `dispatcher -f <config_path>`) to avoid hard-coding input/output directories.


