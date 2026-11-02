# Shared implementation code

This directory contains shared API_PKC helper code and HARE core modules used by
Reference, x86 Optimized and ARM/SVE Additional implementation lines.

```text
api_pkc/       ICCS-style KAT helper and deterministic random generator hooks
hare_core/     shared PKE/KEM, compression, code, GF, vector, RM/RS and platform kernels
```

Generated constants are checked by:

```bash
bash tools/gates/check_generated_artifacts.sh
```
