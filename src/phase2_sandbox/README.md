## Phase 2: sandbox analysis

This directory contains **phase2** programs used to further analyze instruction candidates after phase1 screening.

There are two categories:

- **`sandbox_demos/`**: small demos/templates for validating known behaviors (good for sanity checks).
- **Large-scale analyzers (this directory)**: programs that iterate candidate encodings and produce binary artifacts for offline analysis.

### Naming convention (recommended)

To keep things discoverable for newcomers:

- **Demos/templates** live under `sandbox_demos/` and are meant for *known* instructions or minimal repros.
- **Analyzers** live in the top-level `phase2_sandbox/` directory and are meant for *large-scale* runs over candidate sets.

The current filenames keep historical compatibility. If you add new programs, prefer an explicit prefix like `analyze_` / `scan_` for large-scale analyzers and `demo_` for demos.

### Input conventions

Most analyzers read range files from a local directory named `hidden_insn/`:

- `hidden_insn/resN.txt`
- `hidden_insn/resN_timeout_decoded.txt` (for timeout/control-flow follow-up)

For phase2 analyzers, ranges are typically parsed as **hex**:

```
[0xSTART, 0xEND]
```

The exact parser is `sscanf(..., "[%x, %x]")` in these programs.

### Programs (large-scale analyzers)

#### `fuzzer_arithmetic` (build target: `build/fuzzer_arithmetic`)

- **Goal**: detect instruction encodings that modify GPRs/CPSR/SP-like state.
- **Input**: `hidden_insn/resN.txt`
- **Outputs** (created under `arithmetic_results/`):
  - `resN_complete.bin`: bitmap planes for GPR/CPSR/(SP marker) effects
  - `resN_cpsr.bin`: per-instruction CPSR change logs (when CPSR changes)

#### `fuzzer_memaccess` (build target: `build/fuzzer_memaccess`)

- **Goal**: detect instruction encodings that likely perform loads/stores by using PMU retired events.
- **Input**: `hidden_insn/resN.txt`
- **Outputs** (created under `memaccess_results/`):
  - `resN_complete.bin`
- **Note**: this relies on **user-space PMU access** being enabled on your platform. See `pmu_user_module/`.

#### `fuzzer_simd` (build target: `build/fuzzer_simd`)

- **Goal**: detect instruction encodings that modify SIMD registers or FPSCR/FPSR state.
- **Input**: `hidden_insn/resN.txt`
- **Outputs** (created under `simd_results/`):
  - `resN_complete.bin`
  - `resN_cpsr.bin` (actually FPSCR change logs; filename kept for compatibility)

#### `fuzzer_control_flow` (build target: `build/fuzzer_control_flow`)

- **Goal**: analyze timeout-decoded candidates for control-flow anomalies (loops/deadlocks, jumps out of sandbox, undefined instruction, etc.).
- **Input**: `hidden_insn/resN_timeout_decoded.txt`
- **Output**: prints classification to stdout (no structured output file by default).

### Single-instruction validation: `macro_valid` (build target: `build/macro_valid`)

`macro_valid` is a lightweight helper to validate a **single** instruction encoding by embedding it via a preprocessor macro.

Build with a specific encoding:

```bash
make 0xe1a00001
```

This rebuilds `build/macro_valid` with `-DTEST_INSTRUCTION=<hex>`.

### PMU demos

- `pmu32.c`: AArch32 PMU demo (CP15-based), mainly for experiments/verification.
- `pmu64.c`: AArch64 PMU demo (system-register-based).

### Related docs

- `sandbox_demos/README.md`: purpose of each demo/template program
- `pmu_user_module/README.md`: enabling EL0 PMU access via a kernel module


