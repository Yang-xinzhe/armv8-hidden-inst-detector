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
  - `dispatcher_screen.c`: schedules jobs across cores (fork + CPU affinity). Supports intelligent path deduction based on Platform/Core/ISA/Stage.
  - `worker_screen.c`: executes ranges and writes bitmap results
- **`experiments/`**:
  - `inputs/`: Undefined instruction seeds (e.g., `A32_undef_seeds`).
  - `targets/`: Results organized by Platform/Core/ISA/Stage.
- **`src/phase2_sandbox/`**:
  - analysis programs for large-scale validation/characterization (e.g., `test_arithmetic.c`, `test_memory.c`, `test_simd.c`, `test_control_flow.c`)
  - `demos/`: small demos/templates used to validate known behaviors
  - `pmu_user_module/`: kernel module used to enable user-space PMU access (requires `insmod`)
- **`res/`**: local experiment outputs (ignored by git by default; keep your results here).

### Build

The default toolchain is configured for **AArch32** execution on ARMv8:

- Compiler: `arm-linux-gnueabihf-gcc` (see `CC` in `Makefile`)

Build everything:

```bash
make
```

Outputs are placed under `build/` (e.g., `build/dispatcher`, `build/phase1_screen`, ...).
Demo binaries are placed under `build/demo/`.

### Workflow & Usage

The project uses an intelligent `dispatcher` to manage both the screening (Phase 1) and fuzzing (Phase 2) workflows. It supports automatic path deduction to organize experiments by Platform, Core, and ISA.

#### Dispatcher Arguments

| Flag | Description |
| :--- | :--- |
| `-c <count>` | Number of parallel worker processes. |
| `-x <offset>` | Core ID offset (e.g., `-x 4` to start workers on core 4). |
| `-e <exe>` | Path to the worker executable (Phase 1 or Phase 2 binary). |
| `-P <plat>` | Platform name (e.g., `RK3588`, `RaspberryPi4`). |
| `-C <core>` | CPU Core microarchitecture (e.g., `A76`, `A72`). |
| `-I <isa>` | ISA under test (e.g., `A32`, `T32`). |
| `-S <stage>` | Stage ID: `1` for Screening, `2` for Fuzzing. |
| `-d <dir>` | (Optional) Override input directory. |
| `-r <dir>` | (Optional) Override output directory. |

#### Automatic Path Deduction

If `-P`, `-C`, `-I`, and `-S` are provided, the dispatcher automatically configures directories:

- **Stage 1 (Screening)**:
  - **Input**: `experiments/inputs/<ISA>_undef_seeds`
  - **Output**: `experiments/targets/<P>/<C>/<I>/01_screening`
- **Stage 2 (Fuzzing)**:
  - **Input**: `experiments/targets/<P>/<C>/<I>/01_screening` (Takes output from Stage 1)
  - **Output**: `experiments/targets/<P>/<C>/<I>/02_fuzzing/arithmetic`

#### Example Commands

**Phase 1: Screening**

Run the screening worker on 4 cores (starting at core 0) for RK3588/A76/A32:

```bash
./build/dispatcher \
    -c 4 -x 0 \
    -e ./build/phase1_screen \
    -P RK3588 -C A76 -I A32 -S 1
```

This will read from `experiments/inputs/A32_undef_seeds` and write `candidates_*.bin` to `experiments/targets/RK3588/A76/A32/01_screening`.

**Phase 2: Arithmetic Fuzzing**

Run the arithmetic fuzzer using the results from Phase 1:

```bash
./build/dispatcher \
    -c 4 -x 0 \
    -e ./build/test_arithmetic_a32 \
    -P RK3588 -C A76 -I A32 -S 2
```

This will automatically pick up the screening results and output findings to `02_fuzzing/arithmetic`.

### Phase 2: Sandbox Analysis Details

Phase 2 programs (`test_arithmetic.c`, `test_memory.c`, etc.) are located in `src/phase2_sandbox/`. They are designed to be invoked by the dispatcher but can also be run standalone if input/output directories are manually specified.

See internal documentation for details:
- `src/phase2_sandbox/README.md`
- `src/phase2_sandbox/demos/README.md`
- `src/phase2_sandbox/pmu_user_module/README.md`

### Notes on results (`res/`)

The `res/` directory is intended for **local experiment artifacts** and is ignored by git (except a `.gitkeep` placeholder). This keeps the repository clean while allowing you to store large and messy intermediate outputs locally.

### Troubleshooting

- **“Permission denied” / PMU access issues**: you may need to build and insert the kernel module under `src/phase2_sandbox/pmu_user_module/` and run with proper privileges.
- **Directory not found / hard-coded paths**: use `config/project.conf` (and `dispatcher -f <config_path>`) to avoid hard-coding input/output directories.


