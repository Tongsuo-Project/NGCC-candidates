CTL-769-1024 KEM self-evaluation files for MinGW/Windows

Files:
1. test_kem_correctness_ctl.c
   Correctness test only:
   - fixed deterministic random seed
   - length-interface checks
   - kem_keygen -> kem_enc -> kem_dec
   - compares encapsulated and decapsulated shared secrets
   - writes full KAT vectors to ctl_kem_kat_vectors.txt

2. benchmark_selfeval_ctl.c
   Self-evaluation benchmark:
   - average cycles/op, average time, and throughput for keygen, encapsulation, and decapsulation
   - process baseline memory and peak memory through GetProcessMemoryInfo on Windows
   - public key, private key, ciphertext, and shared-secret sizes
   - writes ctl_kem_benchmark_raw.csv and ctl_kem_benchmark_summary.csv

Build:
Run this directory's Makefile:

mingw32-make all

This keeps the original targets and also builds:

test_kem_correctness_ctl.exe
benchmark_selfeval_ctl.exe

Run:

test_kem_correctness_ctl.exe
benchmark_selfeval_ctl.exe

The benchmark defaults to 100 iterations. You can pass another count:

benchmark_selfeval_ctl.exe 500

Notes:
- benchmark_selfeval_ctl.exe links with -lpsapi for Windows process memory measurement.
- CTL is a KEM. The ciphertext is the encapsulation message.
