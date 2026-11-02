/* Copyright (c) 2026 Hang Zhang. */

/* Seven-stream XOF schedules. */
/* Specification SHA-256: 4f892b32cd6f32f1b986fbd9681388c284acd8e8b50c04822e085beb8406ebb9 */

#ifndef BIT_XOF_SCHEDULES_GENERATED_H
#define BIT_XOF_SCHEDULES_GENERATED_H

#include <stdint.h>

#define BIT_XOF_SCHEDULE_WIDTH 4U
#define BIT_XOF_SCHEDULE_IDLE UINT16_MAX

enum bit_xof_schedule_flags {
    BIT_XOF_SCHEDULE_ADAPTIVE_REFILL = 1U << 0,
    BIT_XOF_SCHEDULE_SCALAR_PREFERRED = 1U << 1
};

typedef struct {
    uint16_t stream[BIT_XOF_SCHEDULE_WIDTH];
    uint32_t first_block[BIT_XOF_SCHEDULE_WIDTH];
    uint32_t blocks;
} bit_xof_schedule_stage;

typedef struct {
    const bit_xof_schedule_stage *stages;
    uint16_t stream_count;
    uint16_t stage_count;
    uint16_t rate_bytes;
    uint16_t flags;
    uint32_t permutations;
    uint32_t useful_lane_blocks;
    uint32_t state_loads;
} bit_xof_schedule_plan;

static const bit_xof_schedule_stage bit80_gaussian_stages[] = {
    {{0U, 1U, BIT_XOF_SCHEDULE_IDLE, BIT_XOF_SCHEDULE_IDLE}, {0U, 0U, 0U, 0U}, 66U},
};
#define BIT80_GAUSSIAN_PERMUTATIONS 66U
#define BIT80_GAUSSIAN_STAGES 1U

static const bit_xof_schedule_stage bit128_gaussian_stages[] = {
    {{0U, 1U, 2U, 3U}, {0U, 0U, 0U, 0U}, 18U},
    {{4U, 5U, BIT_XOF_SCHEDULE_IDLE, BIT_XOF_SCHEDULE_IDLE}, {0U, 0U, 0U, 0U}, 1U},
    {{4U, 5U, 0U, 1U}, {1U, 1U, 18U, 18U}, 17U},
    {{4U, 5U, 2U, 3U}, {18U, 18U, 18U, 18U}, 17U},
};
#define BIT128_GAUSSIAN_PERMUTATIONS 53U
#define BIT128_GAUSSIAN_STAGES 4U

static const bit_xof_schedule_stage bit256_gaussian_stages[] = {
    {{0U, 1U, 2U, 3U}, {0U, 0U, 0U, 0U}, 33U},
    {{4U, 5U, 0U, 1U}, {0U, 0U, 33U, 33U}, 33U},
    {{4U, 5U, 2U, 3U}, {33U, 33U, 33U, 33U}, 33U},
};
#define BIT256_GAUSSIAN_PERMUTATIONS 99U
#define BIT256_GAUSSIAN_STAGES 3U

static const bit_xof_schedule_stage bit512_gaussian_stages[] = {
    {{0U, 1U, 2U, 3U}, {0U, 0U, 0U, 0U}, 66U},
    {{4U, 5U, 0U, 1U}, {0U, 0U, 66U, 66U}, 66U},
    {{4U, 5U, 2U, 3U}, {66U, 66U, 66U, 66U}, 66U},
};
#define BIT512_GAUSSIAN_PERMUTATIONS 198U
#define BIT512_GAUSSIAN_STAGES 3U

static const bit_xof_schedule_stage bit80_s1_stages[] = {
    {{0U, 1U, BIT_XOF_SCHEDULE_IDLE, BIT_XOF_SCHEDULE_IDLE}, {0U, 0U, 0U, 0U}, 3U},
};
#define BIT80_S1_PERMUTATIONS 3U
#define BIT80_S1_STAGES 1U

static const bit_xof_schedule_stage bit128_s1_stages[] = {
    {{0U, 1U, 2U, 3U}, {0U, 0U, 0U, 0U}, 1U},
    {{4U, 5U, BIT_XOF_SCHEDULE_IDLE, BIT_XOF_SCHEDULE_IDLE}, {0U, 0U, 0U, 0U}, 1U},
};
#define BIT128_S1_PERMUTATIONS 2U
#define BIT128_S1_STAGES 2U

static const bit_xof_schedule_stage bit256_s1_stages[] = {
    {{0U, 1U, 2U, 3U}, {0U, 0U, 0U, 0U}, 1U},
    {{4U, 5U, BIT_XOF_SCHEDULE_IDLE, BIT_XOF_SCHEDULE_IDLE}, {0U, 0U, 0U, 0U}, 1U},
};
#define BIT256_S1_PERMUTATIONS 2U
#define BIT256_S1_STAGES 2U

static const bit_xof_schedule_stage bit512_s1_stages[] = {
    {{0U, 1U, 2U, 3U}, {0U, 0U, 0U, 0U}, 1U},
    {{0U, 1U, 4U, 5U}, {1U, 1U, 0U, 0U}, 1U},
    {{2U, 3U, 4U, 5U}, {1U, 1U, 1U, 1U}, 1U},
};
#define BIT512_S1_PERMUTATIONS 3U
#define BIT512_S1_STAGES 3U

static const bit_xof_schedule_stage bit80_expanda_stages[] = {
    {{0U, BIT_XOF_SCHEDULE_IDLE, BIT_XOF_SCHEDULE_IDLE, BIT_XOF_SCHEDULE_IDLE}, {0U, 0U, 0U, 0U}, 6U},
};
#define BIT80_EXPANDA_PERMUTATIONS 6U
#define BIT80_EXPANDA_STAGES 1U

static const bit_xof_schedule_stage bit128_expanda_stages[] = {
    {{0U, 1U, 2U, 3U}, {0U, 0U, 0U, 0U}, 3U},
    {{4U, 5U, 6U, BIT_XOF_SCHEDULE_IDLE}, {0U, 0U, 0U, 0U}, 1U},
    {{4U, 5U, 7U, 8U}, {1U, 1U, 0U, 0U}, 1U},
    {{4U, 6U, 7U, 8U}, {2U, 1U, 1U, 1U}, 1U},
    {{5U, 6U, 7U, 8U}, {2U, 2U, 2U, 2U}, 1U},
};
#define BIT128_EXPANDA_PERMUTATIONS 7U
#define BIT128_EXPANDA_STAGES 5U

static const bit_xof_schedule_stage bit256_expanda_stages[] = {
    {{0U, 1U, 2U, 3U}, {0U, 0U, 0U, 0U}, 8U},
    {{4U, 5U, 6U, 7U}, {0U, 0U, 0U, 0U}, 2U},
    {{4U, 5U, 6U, 8U}, {2U, 2U, 2U, 0U}, 2U},
    {{4U, 5U, 7U, 8U}, {4U, 4U, 2U, 2U}, 2U},
    {{4U, 6U, 7U, 8U}, {6U, 4U, 4U, 4U}, 2U},
    {{5U, 6U, 7U, 8U}, {6U, 6U, 6U, 6U}, 2U},
};
#define BIT256_EXPANDA_PERMUTATIONS 18U
#define BIT256_EXPANDA_STAGES 6U

static const bit_xof_schedule_stage bit512_expanda_stages[] = {
    {{0U, 1U, 2U, 3U}, {0U, 0U, 0U, 0U}, 24U},
    {{4U, 5U, 6U, 7U}, {0U, 0U, 0U, 0U}, 6U},
    {{4U, 5U, 6U, 8U}, {6U, 6U, 6U, 0U}, 6U},
    {{4U, 5U, 7U, 8U}, {12U, 12U, 6U, 6U}, 6U},
    {{4U, 6U, 7U, 8U}, {18U, 12U, 12U, 12U}, 6U},
    {{5U, 6U, 7U, 8U}, {18U, 18U, 18U, 18U}, 6U},
};
#define BIT512_EXPANDA_PERMUTATIONS 54U
#define BIT512_EXPANDA_STAGES 6U

enum bit_xof_schedule_id {
    BIT_XOF_SCHEDULE_BIT80_GAUSSIAN = 0,
    BIT_XOF_SCHEDULE_BIT128_GAUSSIAN = 1,
    BIT_XOF_SCHEDULE_BIT256_GAUSSIAN = 2,
    BIT_XOF_SCHEDULE_BIT512_GAUSSIAN = 3,
    BIT_XOF_SCHEDULE_BIT80_S1 = 4,
    BIT_XOF_SCHEDULE_BIT128_S1 = 5,
    BIT_XOF_SCHEDULE_BIT256_S1 = 6,
    BIT_XOF_SCHEDULE_BIT512_S1 = 7,
    BIT_XOF_SCHEDULE_BIT80_EXPANDA = 8,
    BIT_XOF_SCHEDULE_BIT128_EXPANDA = 9,
    BIT_XOF_SCHEDULE_BIT256_EXPANDA = 10,
    BIT_XOF_SCHEDULE_BIT512_EXPANDA = 11,
    BIT_XOF_SCHEDULE_COUNT = 12
};

static const bit_xof_schedule_plan bit_xof_schedules[BIT_XOF_SCHEDULE_COUNT] = {
    {bit80_gaussian_stages, 2U, 1U, 136U, BIT_XOF_SCHEDULE_ADAPTIVE_REFILL, 66U, 132U, 0U},
    {bit128_gaussian_stages, 6U, 4U, 136U, BIT_XOF_SCHEDULE_ADAPTIVE_REFILL, 53U, 210U, 6U},
    {bit256_gaussian_stages, 6U, 3U, 136U, BIT_XOF_SCHEDULE_ADAPTIVE_REFILL, 99U, 396U, 4U},
    {bit512_gaussian_stages, 6U, 3U, 136U, BIT_XOF_SCHEDULE_ADAPTIVE_REFILL, 198U, 792U, 4U},
    {bit80_s1_stages, 2U, 1U, 136U, 0U, 3U, 6U, 0U},
    {bit128_s1_stages, 6U, 2U, 136U, 0U, 2U, 6U, 2U},
    {bit256_s1_stages, 6U, 2U, 136U, 0U, 2U, 6U, 2U},
    {bit512_s1_stages, 6U, 3U, 136U, 0U, 3U, 12U, 4U},
    {bit80_expanda_stages, 1U, 1U, 168U, BIT_XOF_SCHEDULE_ADAPTIVE_REFILL | BIT_XOF_SCHEDULE_SCALAR_PREFERRED, 6U, 6U, 0U},
    {bit128_expanda_stages, 9U, 5U, 168U, BIT_XOF_SCHEDULE_ADAPTIVE_REFILL, 7U, 27U, 7U},
    {bit256_expanda_stages, 9U, 6U, 168U, BIT_XOF_SCHEDULE_ADAPTIVE_REFILL, 18U, 72U, 8U},
    {bit512_expanda_stages, 9U, 6U, 168U, BIT_XOF_SCHEDULE_ADAPTIVE_REFILL, 54U, 216U, 8U},
};

#endif
