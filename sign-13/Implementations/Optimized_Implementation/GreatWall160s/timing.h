#ifndef FAEST_TIMING_H
#define FAEST_TIMING_H

#ifndef FAEST_ENABLE_TIMING
#define FAEST_ENABLE_TIMING 0
#endif

#ifndef ENC_CONSTRAINTS_ENABLE_TIMING
#define ENC_CONSTRAINTS_ENABLE_TIMING 0
#endif

#ifndef TIMING_AVG_RUNS
#define TIMING_AVG_RUNS 1
#endif

#if FAEST_ENABLE_TIMING || ENC_CONSTRAINTS_ENABLE_TIMING

#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

static inline uint64_t faest_timing_now_us(void)
{
  struct timeval tv;
  gettimeofday(&tv, 0);
  return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

#endif

#if FAEST_ENABLE_TIMING

#define FAEST_TIMING_DECLARE() uint64_t faest_timing_start_us_ = 0
#define FAEST_TIMING_START() do { faest_timing_start_us_ = faest_timing_now_us(); } while (0)
#define FAEST_TIMING_PRINT(label)                                                       \
  do {                                                                                 \
    const uint64_t faest_timing_end_us_ = faest_timing_now_us();                      \
    static uint64_t faest_timing_total_us_ = 0;                                        \
    static size_t faest_timing_count_ = 0;                                             \
    faest_timing_total_us_ += faest_timing_end_us_ - faest_timing_start_us_;           \
    if (++faest_timing_count_ == TIMING_AVG_RUNS) {                                    \
      fprintf(stderr, "[time-avg] %s: %.3f ms (n=%zu)\n", (label),                 \
              faest_timing_total_us_ / (1000.0 * faest_timing_count_),                 \
              faest_timing_count_);                                                    \
      faest_timing_total_us_ = 0;                                                      \
      faest_timing_count_ = 0;                                                        \
    }                                                                                  \
  } while (0)

#else

#define FAEST_TIMING_DECLARE() ((void)0)
#define FAEST_TIMING_START() ((void)0)
#define FAEST_TIMING_PRINT(label) ((void)0)

#endif

#if ENC_CONSTRAINTS_ENABLE_TIMING

#define ENC_TIMING_DECLARE() uint64_t enc_timing_start_us_ = 0
#define ENC_TIMING_START() do { enc_timing_start_us_ = faest_timing_now_us(); } while (0)
#define ENC_TIMING_PRINT(label)                                                       \
  do {                                                                                \
    const uint64_t enc_timing_end_us_ = faest_timing_now_us();                       \
    static uint64_t enc_timing_total_us_ = 0;                                         \
    static size_t enc_timing_count_ = 0;                                              \
    enc_timing_total_us_ += enc_timing_end_us_ - enc_timing_start_us_;                \
    if (++enc_timing_count_ == TIMING_AVG_RUNS) {                                     \
      fprintf(stderr, "[enc-time-avg] %s: %.3f ms (n=%zu)\n", (label),            \
              enc_timing_total_us_ / (1000.0 * enc_timing_count_),                    \
              enc_timing_count_);                                                     \
      enc_timing_total_us_ = 0;                                                       \
      enc_timing_count_ = 0;                                                         \
    }                                                                                 \
  } while (0)
#define ENC_TIMING_PRINT_PROVER(state, label)                                         \
  do { if (!(state)->verifier) ENC_TIMING_PRINT(label); } while (0)

#else

#define ENC_TIMING_DECLARE() ((void)0)
#define ENC_TIMING_START() ((void)0)
#define ENC_TIMING_PRINT(label) ((void)0)
#define ENC_TIMING_PRINT_PROVER(state, label) ((void)0)

#endif

#endif
