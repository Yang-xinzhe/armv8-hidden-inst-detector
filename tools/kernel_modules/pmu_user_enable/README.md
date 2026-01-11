## pmu_user_module: enable user-space PMU access (EL0)

Some phase2 experiments (e.g., `test_memory`) rely on reading PMU counters from **user space** to infer load/store behavior.

On many ARMv8 platforms, EL0 access to PMU registers is disabled by default and must be enabled by the kernel (EL1). This directory provides a small kernel module that enables EL0 PMU access on all CPUs by configuring:

- `PMUSERENR_EL0` (allow EL0 PMU access)
- `PMCR_EL0` / `PMCNTENSET_EL0` (enable and reset counters)

### Requirements

- A running kernel that supports loadable modules.
- Kernel build tree/headers matching the target kernel.
- Root privileges on the target (for `insmod`/`rmmod`).

### Build

The module Makefile expects:

- `KDIR`: path to your kernel source tree (or headers tree)
- `ARCH`: default `arm64`
- `CROSS_COMPILE`: default `aarch64-linux-gnu-`

Example:

```bash
make KDIR=/path/to/linux ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-
```

This produces `pmu_user.ko`.

### Install / Uninstall

On the target device:

```bash
sudo insmod pmu_user.ko
dmesg | tail
```

To remove:

```bash
sudo rmmod pmu_user
dmesg | tail
```

### Verification

If insertion succeeds, the kernel log should show messages like:

- `pmu_user: enabling PMU access from EL0 on all CPUs`
- `pmu_user: CPUx pmu enabled for EL0`

Then user-space PMU code (e.g., `src/phase2_sandbox/demos/demo_pmu.c` or the `test_memory` analyzer) should be able to read counters without trapping.

### Security note

Enabling EL0 PMU access may have security/performance implications on shared systems. Only use this on controlled research setups.



