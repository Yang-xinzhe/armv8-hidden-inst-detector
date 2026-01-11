## Analysis Tools

This directory contains Python scripts for analyzing the results produced by Phase 1 (Screening) and Phase 2 (Sandbox Fuzzing).

### `phase1_decode.py`

Decodes the raw bitmap outputs from Phase 1 (`candidates_*.bin`, `timeout_*.bin`) into human-readable lists or CSVs.

```bash
python3 tools/analysis/phase1_decode.py \
    --input <phase1_output_dir> \
    --bin-out <binary_output_dir> \
    --csv-out <csv_output_dir>
```

Typically used if you want to inspect the raw findings from the screening phase manually.