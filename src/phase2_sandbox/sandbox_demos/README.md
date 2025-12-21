## Sandbox demos & templates

This directory contains **small demos/templates** that exercise known instruction behaviors inside the sandbox runtime.

These are useful for:

- sanity-checking the sandbox execution path
- validating template assembly/ABI assumptions
- creating minimal repros for a specific instruction class

### Files

#### `regs_template.c` + `regs_template_asm.S` (build target: `build/regs_demo`)

- **Purpose**: run an instruction and record general register state before/after.
- **Typical use**: validate that the sandbox correctly preserves/restores registers and that you can observe side-effects.

#### `pmu_template.c` + `pmu_template_asm.S` (build target: `build/pmu_demo`)

- **Purpose**: run an instruction while collecting PMU-derived load/store signals.
- **Typical use**: validate your PMU setup (including `pmu_user_module`) and the PMU readout path.

#### `simd_template.c` + `simd_template_asm.S` (build target: `build/simd_demo`)

- **Purpose**: run an instruction and capture SIMD/FPSCR state transitions.
- **Typical use**: verify SIMD state save/restore and detection logic.

#### `control_flow.c` + `control_flow_asm.S` (build target: `build/control_flow_demo`)

- **Purpose**: minimal control-flow template that can be used to reason about trap/PC behavior around a tested instruction.
- **Typical use**: confirm “normal sequential execution” vs “undefined” vs “jump/escape” signal patterns.

### How these relate to phase2 analyzers

The demos/templates focus on *known* behaviors and minimal scaffolding, while the top-level programs in `src/phase2_sandbox/` (e.g., `arithmetic`, `memaccess`, `simd`, `controlFlow`) are intended for **large-scale** runs over many candidates and producing structured artifacts.



