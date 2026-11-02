#include "rsencode.h"
#include "rsencode_common.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum
{
    kFieldBits = 16,
    kFieldSize = 1 << kFieldBits,
    kFieldOrder = kFieldSize - 1,
    kPrimitive = 0x1002D,
    kGenerator = 2,
    kFixedOriginalCount = 11,
    kFixedRecoveryCount = 44
};

static int g_initialized = 0;
static int g_tables_ready = 0;
static int g_fixed_self_test_ready = 0;
static uint16_t g_exp_table[(kFieldOrder * 2)];
static uint16_t g_log_table[kFieldSize];

static size_t elements_per_block(uint64_t buffer_bytes);

static uint16_t mul_basic(uint16_t a, uint16_t b)
{
    int i;
    uint16_t result = 0;

    for (i = 0; i < kFieldBits; ++i)
    {
        if ((b & 1u) != 0u)
        {
            result ^= a;
        }
        if ((a & 0x8000u) != 0u)
        {
            a = (uint16_t)(a << 1);
            a ^= (uint16_t)(kPrimitive & 0xFFFFu);
        }
        else
        {
            a = (uint16_t)(a << 1);
        }
        b = (uint16_t)(b >> 1);
    }
    return result;
}

static void ensure_tables(void)
{
    uint16_t value;
    uint32_t i;

    if (g_tables_ready)
    {
        return;
    }

    value = 1;
    for (i = 0; i < (uint32_t)kFieldOrder; ++i)
    {
        g_exp_table[i] = value;
        g_log_table[value] = (uint16_t)i;
        value = mul_basic(value, (uint16_t)kGenerator);
    }
    for (i = (uint32_t)kFieldOrder; i < (uint32_t)(kFieldOrder * 2); ++i)
    {
        g_exp_table[i] = g_exp_table[i - kFieldOrder];
    }
    g_log_table[0] = 0;
    g_tables_ready = 1;
}

static uint16_t gf_add(uint16_t a, uint16_t b)
{
    return (uint16_t)(a ^ b);
}

static uint16_t gf_mul(uint16_t a, uint16_t b)
{
    uint32_t sum;

    if (a == 0u || b == 0u)
    {
        return 0u;
    }
    sum = (uint32_t)g_log_table[a] + (uint32_t)g_log_table[b];
    return g_exp_table[sum];
}

static uint16_t generator_pow(uint32_t exponent)
{
    return g_exp_table[exponent % (uint32_t)kFieldOrder];
}

static RsencodeResult rs_encode_scalar(
    uint64_t buffer_bytes,
    unsigned original_count,
    unsigned recovery_count,
    unsigned work_count,
    const void *const *original_data,
    void **work_data)
{
    size_t symbol_count;
    unsigned shard;
    unsigned p;
    size_t s;

    if (buffer_bytes == 0u || (buffer_bytes % 2u) != 0u)
    {
        return Rsencode_InvalidSize;
    }
    if (original_count == 0u || recovery_count == 0u || work_count < recovery_count)
    {
        return Rsencode_InvalidCounts;
    }
    if (original_data == NULL || work_data == NULL)
    {
        return Rsencode_InvalidInput;
    }

    ensure_tables();
    symbol_count = elements_per_block(buffer_bytes);

    const uint16_t *data_shards[original_count];
    uint16_t *parity_shards[recovery_count];
    uint16_t eval_points[recovery_count];

    for (shard = 0; shard < original_count; ++shard)
    {
        if (original_data[shard] == NULL)
        {
            return Rsencode_InvalidInput;
        }
        data_shards[shard] = (const uint16_t *)original_data[shard];
    }
    for (p = 0; p < recovery_count; ++p)
    {
        if (work_data[p] == NULL)
        {
            return Rsencode_InvalidInput;
        }
        parity_shards[p] = (uint16_t *)work_data[p];
        eval_points[p] = generator_pow((uint32_t)p + 1u);
    }
    for (p = 0; p < recovery_count; ++p)
    {
        uint16_t *parity = parity_shards[p];
        uint16_t eval_point = eval_points[p];
        int coeff;

        memset(parity, 0, buffer_bytes);
        for (coeff = (int)original_count - 1; coeff >= 0; --coeff)
        {
            const uint16_t *data = data_shards[coeff];

            for (s = 0; s < symbol_count; ++s)
            {
                parity[s] = gf_add(gf_mul(parity[s], eval_point), data[s]);
            }
        }
    }

    return Rsencode_Success;
}

static RsencodeResult rs_encode_fixed_specialized(
    uint64_t buffer_bytes,
    unsigned work_count,
    const void *const *original_data,
    void **work_data)
{
    const uint16_t *data_shards[kFixedOriginalCount];
    uint16_t *parity_shards[kFixedRecoveryCount];
    size_t symbol_count;
    unsigned shard;
    size_t s;

    if (work_count < kFixedRecoveryCount)
    {
        return Rsencode_InvalidCounts;
    }

    for (shard = 0; shard < kFixedOriginalCount; ++shard)
    {
        if (original_data[shard] == NULL)
        {
            return Rsencode_InvalidInput;
        }
        data_shards[shard] = (const uint16_t *)original_data[shard];
    }
    for (shard = 0; shard < kFixedRecoveryCount; ++shard)
    {
        if (work_data[shard] == NULL)
        {
            return Rsencode_InvalidInput;
        }
        parity_shards[shard] = (uint16_t *)work_data[shard];
    }

    symbol_count = elements_per_block(buffer_bytes);
    for (s = 0; s < symbol_count; ++s)
    {
        uint16_t v0 = data_shards[0][s];
        uint16_t v1 = data_shards[1][s];
        uint16_t v2 = data_shards[2][s];
        uint16_t v3 = data_shards[3][s];
        uint16_t v4 = data_shards[4][s];
        uint16_t v5 = data_shards[5][s];
        uint16_t v6 = data_shards[6][s];
        uint16_t v7 = data_shards[7][s];
        uint16_t v8 = data_shards[8][s];
        uint16_t v9 = data_shards[9][s];
        uint16_t v10 = data_shards[10][s];
        uint32_t exp1 = 0u;
        uint32_t exp2 = 0u;
        uint32_t exp3 = 0u;
        uint32_t exp4 = 0u;
        uint32_t exp5 = 0u;
        uint32_t exp6 = 0u;
        uint32_t exp7 = 0u;
        uint32_t exp8 = 0u;
        uint32_t exp9 = 0u;
        uint32_t exp10 = 0u;
        int nz1 = v1 != 0u;
        int nz2 = v2 != 0u;
        int nz3 = v3 != 0u;
        int nz4 = v4 != 0u;
        int nz5 = v5 != 0u;
        int nz6 = v6 != 0u;
        int nz7 = v7 != 0u;
        int nz8 = v8 != 0u;
        int nz9 = v9 != 0u;
        int nz10 = v10 != 0u;
        unsigned p;

        if (nz1) exp1 = (uint32_t)g_log_table[v1] + 1u;
        if (nz2) exp2 = (uint32_t)g_log_table[v2] + 2u;
        if (nz3) exp3 = (uint32_t)g_log_table[v3] + 3u;
        if (nz4) exp4 = (uint32_t)g_log_table[v4] + 4u;
        if (nz5) exp5 = (uint32_t)g_log_table[v5] + 5u;
        if (nz6) exp6 = (uint32_t)g_log_table[v6] + 6u;
        if (nz7) exp7 = (uint32_t)g_log_table[v7] + 7u;
        if (nz8) exp8 = (uint32_t)g_log_table[v8] + 8u;
        if (nz9) exp9 = (uint32_t)g_log_table[v9] + 9u;
        if (nz10) exp10 = (uint32_t)g_log_table[v10] + 10u;

        for (p = 0; p < kFixedRecoveryCount; ++p)
        {
            uint16_t acc = v0;

            if (nz1)
            {
                acc ^= g_exp_table[exp1];
                exp1 += 1u;
            }
            if (nz2)
            {
                acc ^= g_exp_table[exp2];
                exp2 += 2u;
            }
            if (nz3)
            {
                acc ^= g_exp_table[exp3];
                exp3 += 3u;
            }
            if (nz4)
            {
                acc ^= g_exp_table[exp4];
                exp4 += 4u;
            }
            if (nz5)
            {
                acc ^= g_exp_table[exp5];
                exp5 += 5u;
            }
            if (nz6)
            {
                acc ^= g_exp_table[exp6];
                exp6 += 6u;
            }
            if (nz7)
            {
                acc ^= g_exp_table[exp7];
                exp7 += 7u;
            }
            if (nz8)
            {
                acc ^= g_exp_table[exp8];
                exp8 += 8u;
            }
            if (nz9)
            {
                acc ^= g_exp_table[exp9];
                exp9 += 9u;
            }
            if (nz10)
            {
                acc ^= g_exp_table[exp10];
                exp10 += 10u;
            }
            parity_shards[p][s] = acc;
        }
    }

    return Rsencode_Success;
}

static int is_fixed_specialized_shape(
    uint64_t buffer_bytes,
    unsigned original_count,
    unsigned recovery_count,
    unsigned work_count)
{
    return buffer_bytes != 0u &&
           (buffer_bytes % 2u) == 0u &&
           original_count == kFixedOriginalCount &&
           recovery_count == kFixedRecoveryCount &&
           work_count >= kFixedRecoveryCount;
}

static RsencodeResult self_test_fixed_specialized(void)
{
    enum
    {
        kSelfTestSymbols = 288
    };

    uint16_t original_storage[kFixedOriginalCount][kSelfTestSymbols];
    uint16_t fast_storage[kFixedRecoveryCount][kSelfTestSymbols];
    uint16_t scalar_storage[kFixedRecoveryCount][kSelfTestSymbols];
    const void *original_ptrs[kFixedOriginalCount];
    void *fast_ptrs[kFixedRecoveryCount];
    void *scalar_ptrs[kFixedRecoveryCount];
    unsigned shard;
    unsigned symbol;
    RsencodeResult fast_result;
    RsencodeResult scalar_result;

    if (g_fixed_self_test_ready)
    {
        return Rsencode_Success;
    }

    for (shard = 0; shard < kFixedOriginalCount; ++shard)
    {
        original_ptrs[shard] = original_storage[shard];
        for (symbol = 0; symbol < kSelfTestSymbols; ++symbol)
        {
            original_storage[shard][symbol] = (uint16_t)(shard * 577u + symbol * 29u + 17u);
        }
    }
    for (shard = 0; shard < kFixedRecoveryCount; ++shard)
    {
        memset(fast_storage[shard], 0, sizeof(fast_storage[shard]));
        memset(scalar_storage[shard], 0, sizeof(scalar_storage[shard]));
        fast_ptrs[shard] = fast_storage[shard];
        scalar_ptrs[shard] = scalar_storage[shard];
    }

    fast_result = rs_encode_fixed_specialized(
        (uint64_t)(kSelfTestSymbols * sizeof(uint16_t)),
        kFixedRecoveryCount,
        original_ptrs,
        fast_ptrs);
    scalar_result = rs_encode_scalar(
        (uint64_t)(kSelfTestSymbols * sizeof(uint16_t)),
        kFixedOriginalCount,
        kFixedRecoveryCount,
        kFixedRecoveryCount,
        original_ptrs,
        scalar_ptrs);
    if (fast_result != Rsencode_Success || scalar_result != Rsencode_Success)
    {
        return Rsencode_Platform;
    }
    if (memcmp(fast_storage, scalar_storage, sizeof(fast_storage)) != 0)
    {
        return Rsencode_Platform;
    }

    g_fixed_self_test_ready = 1;
    return Rsencode_Success;
}

static size_t elements_per_block(uint64_t buffer_bytes)
{
    return (size_t)(buffer_bytes / sizeof(uint16_t));
}

int rs_init_(int version)
{
    RsencodeResult result;

    if (version != RS_VERSION)
    {
        return Rsencode_InvalidInput;
    }

    ensure_tables();
    InitializeCPUArch();
    g_initialized = 1;
    result = self_test_fixed_specialized();
    if (result != Rsencode_Success)
    {
        return result;
    }
    return Rsencode_Success;
}

const char *rs_result_string(RsencodeResult result)
{
    switch (result)
    {
    case Rsencode_Success:
        return "Operation succeeded";
    case Rsencode_NeedMoreData:
        return "Not enough recovery data received";
    case Rsencode_TooMuchData:
        return "Buffer counts are too high";
    case Rsencode_InvalidSize:
        return "Buffer size must be a multiple of 2 bytes";
    case Rsencode_InvalidCounts:
        return "Invalid counts provided";
    case Rsencode_InvalidInput:
        return "A function parameter was invalid";
    case Rsencode_Platform:
        return "Platform is unsupported";
    case Rsencode_CallInitialize:
        return "Call rs_init() first";
    default:
        return "Unknown";
    }
}

unsigned rs_encode_work_count(unsigned original_count, unsigned recovery_count)
{
    if (original_count == 0u || recovery_count == 0u)
    {
        return 0u;
    }
    return recovery_count;
}

RsencodeResult rs_encode(
    uint64_t buffer_bytes,
    unsigned original_count,
    unsigned recovery_count,
    unsigned work_count,
    const void *const *original_data,
    void **work_data)
{
    if (!g_initialized)
    {
        return Rsencode_CallInitialize;
    }
    if (buffer_bytes == 0u || (buffer_bytes % 2u) != 0u)
    {
        return Rsencode_InvalidSize;
    }
    if (original_count == 0u || recovery_count == 0u || work_count < recovery_count)
    {
        return Rsencode_InvalidCounts;
    }
    if (original_data == NULL || work_data == NULL)
    {
        return Rsencode_InvalidInput;
    }
    if (is_fixed_specialized_shape(buffer_bytes, original_count, recovery_count, work_count))
    {
        return rs_encode_fixed_specialized(buffer_bytes, work_count, original_data, work_data);
    }
    return rs_encode_scalar(
        buffer_bytes,
        original_count,
        recovery_count,
        work_count,
        original_data,
        work_data);
}

/* ==========================================================================
 * Vector Reed-Solomon evaluation over the submitted evaluation domain.
 *
 * See rsencode.h for the domain definition and the derivation of the NTT.
 * Everything here is portable C99, single threaded, and free of intrinsics.
 * ========================================================================== */

enum
{
    kEvalOrder = 51,     /* |H| = 3 * 17                                         */
    kEvalMaxCosets = 400,/* ceil(19824 / 51) = 389, with headroom                */
    kEvalMaxFold = 25,   /* ceil(1239 / 51) = 25                                 */
    kEvalMaxPositions = 2048  /* >= N for every submitted instance                 */
};

/*
 * The transform is memory bound: every GF(2^16) product costs table lookups.
 * Every multiplier used below is a fixed precomputed constant, so the tables
 * hold the *logarithm* of the multiplier and each product costs two lookups
 * instead of three.
 */
static uint16_t g_eval_m17log[17][17];                   /* (omega^3)^(i*j)    */
static uint16_t g_eval_twlog[17][3];                     /* omega^(n2*k1)      */
static uint16_t g_eval_w3log;                            /* omega^17, order 3  */
/* Per-coset chirp, fold, and the omega^(i*r) table used by rs_eval_positions. */
static uint16_t g_eval_chirplog[kEvalMaxCosets][kEvalOrder];  /* (g^j)^r       */
static uint16_t g_eval_foldlog[kEvalMaxCosets][kEvalMaxFold]; /* (g^(51j))^t   */
static uint16_t g_eval_wlog[kEvalOrder][kEvalOrder];
static int g_eval_ready = 0;

/* Product with a multiplier whose logarithm is already known. */
static uint16_t gf_mul_log(uint16_t a, uint16_t log_b)
{
    if (a == 0u)
    {
        return 0u;
    }
    return g_exp_table[(uint32_t)g_log_table[a] + (uint32_t)log_b];
}

/* base^e in GF(2^16), with the zero/one cases handled explicitly. */
static uint16_t eval_pow(uint16_t base, unsigned e)
{
    if (e == 0u || base == 1u)
    {
        return 1u;
    }
    if (base == 0u)
    {
        return 0u;
    }
    return g_exp_table[(uint32_t)(((uint64_t)g_log_table[base] * (uint64_t)e) % (uint64_t)kFieldOrder)];
}

/*
 * Precompute the NTT constants and the per-coset chirp factors.  The domain is
 * public and fixed, so this runs once per process.
 */
static void ensure_eval_tables(void)
{
    unsigned i, j, r;
    uint16_t omega;

    if (g_eval_ready)
    {
        return;
    }
    ensure_tables();

    /* omega generates the order-51 subgroup H of GF(2^16)^*. */
    omega = generator_pow(1285u);

    g_eval_w3log = g_log_table[eval_pow(omega, 17u)];
    for (i = 0; i < 17u; ++i)
    {
        for (j = 0; j < 17u; ++j)
        {
            g_eval_m17log[i][j] = g_log_table[eval_pow(eval_pow(omega, 3u), i * j)];
        }
    }
    for (i = 0; i < 17u; ++i)
    {
        for (j = 0; j < 3u; ++j)
        {
            g_eval_twlog[i][j] = g_log_table[eval_pow(omega, i * j)];
        }
    }

    for (j = 0; j < (unsigned)kEvalMaxCosets; ++j)
    {
        uint16_t c = generator_pow(j);
        uint16_t d = eval_pow(c, kEvalOrder);
        uint16_t v = 1u;
        uint16_t u = 1u;

        for (r = 0; r < (unsigned)kEvalOrder; ++r)
        {
            g_eval_chirplog[j][r] = g_log_table[v];
            v = gf_mul(v, c);
        }
        for (r = 0; r < (unsigned)kEvalMaxFold; ++r)
        {
            g_eval_foldlog[j][r] = g_log_table[u];
            u = gf_mul(u, d);
        }
    }

    for (i = 0; i < (unsigned)kEvalOrder; ++i)
    {
        uint16_t wi = eval_pow(omega, i);
        uint16_t v = 1u;

        for (r = 0; r < (unsigned)kEvalOrder; ++r)
        {
            g_eval_wlog[i][r] = g_log_table[v];
            v = gf_mul(v, wi);
        }
    }

    g_eval_ready = 1;
}

/*
 * A primitive cube root satisfies w^2 + w + 1 = 0, i.e. w^2 = w + 1, so with
 * t = x1 + x2 and u = w*t:
 *   X0 = x0 + t,  X1 = (x0 + x2) + u,  X2 = (x0 + x1) + u.
 * One multiplication instead of nine.
 */
static void eval_dft3(uint16_t out[3], const uint16_t in[3])
{
    const uint16_t t = gf_add(in[1], in[2]);
    const uint16_t u = gf_mul_log(t, g_eval_w3log);

    out[0] = gf_add(in[0], t);
    out[1] = gf_add(gf_add(in[0], in[2]), u);
    out[2] = gf_add(gf_add(in[0], in[1]), u);
}

static void eval_dft17(uint16_t out[17], const uint16_t in[17])
{
    unsigned k, n;
    for (k = 0; k < 17u; ++k)
    {
        uint16_t acc = 0u;
        for (n = 0; n < 17u; ++n)
        {
            acc = gf_add(acc, gf_mul_log(in[n], g_eval_m17log[k][n]));
        }
        out[k] = acc;
    }
}

/*
 * Length-51 DFT over H, in the order omega^0 .. omega^50.
 * Cooley-Tukey with 51 = 3 * 17:
 *   X[k1 + 3*k2] = sum_{n2<17} w17^(n2*k2) * omega^(n2*k1)
 *                              * ( sum_{n1<3} w3^(n1*k1) x[17*n1 + n2] )
 */
static void eval_ntt51(uint16_t out[kEvalOrder], const uint16_t in[kEvalOrder])
{
    uint16_t stage[3][17];
    unsigned n1, n2, k1, k2;

    for (n2 = 0; n2 < 17u; ++n2)
    {
        uint16_t col[3];
        uint16_t res[3];

        for (n1 = 0; n1 < 3u; ++n1)
        {
            col[n1] = in[17u * n1 + n2];
        }
        eval_dft3(res, col);
        for (k1 = 0; k1 < 3u; ++k1)
        {
            stage[k1][n2] = res[k1];
        }
    }

    for (k1 = 0; k1 < 3u; ++k1)
    {
        uint16_t col[17];
        uint16_t res[17];

        for (n2 = 0; n2 < 17u; ++n2)
        {
            col[n2] = gf_mul_log(stage[k1][n2], g_eval_twlog[n2][k1]);
        }
        eval_dft17(res, col);
        for (k2 = 0; k2 < 17u; ++k2)
        {
            out[k1 + 3u * k2] = res[k2];
        }
    }
}

/*
 * Fold and chirp the coefficients of one lane into the kEvalOrder values whose
 * DFT gives the codeword on coset j.  The fold is what keeps the transform at
 * length 51 even when the message is longer.
 */
static void eval_fold_coset(
    uint16_t a[kEvalOrder],
    const uint16_t *msg,
    unsigned d,
    unsigned lanes,
    unsigned lane,
    unsigned j)
{
    const uint16_t *chirp = g_eval_chirplog[j];
    const uint16_t *fold = g_eval_foldlog[j];
    const unsigned tmax = (d + (unsigned)kEvalOrder - 1u) / (unsigned)kEvalOrder;
    unsigned r, t;

    for (r = 0; r < (unsigned)kEvalOrder; ++r)
    {
        uint16_t acc = 0u;
        for (t = 0; t < tmax; ++t)
        {
            const unsigned idx = r + (unsigned)kEvalOrder * t;
            if (idx < d)
            {
                acc = gf_add(acc, gf_mul_log(msg[(size_t)idx * lanes + lane], fold[t]));
            }
        }
        a[r] = gf_mul_log(acc, chirp[r]);
    }
}

static RsencodeResult rs_eval_codeword_scalar(
    uint16_t *out,
    const uint16_t *msg,
    unsigned d,
    unsigned n,
    unsigned lanes)
{
    unsigned cosets, j, lane, i;

    if (out == NULL || msg == NULL || lanes == 0u || d == 0u || n == 0u)
    {
        return Rsencode_InvalidInput;
    }
    cosets = (n + (unsigned)kEvalOrder - 1u) / (unsigned)kEvalOrder;
    if (cosets > (unsigned)kEvalMaxCosets)
    {
        return Rsencode_TooMuchData;
    }
    if ((d + (unsigned)kEvalOrder - 1u) / (unsigned)kEvalOrder > (unsigned)kEvalMaxFold)
    {
        return Rsencode_TooMuchData;
    }

    ensure_eval_tables();

    for (lane = 0; lane < lanes; ++lane)
    {
        for (j = 0; j < cosets; ++j)
        {
            uint16_t folded[kEvalOrder];
            uint16_t spectrum[kEvalOrder];

            eval_fold_coset(folded, msg, d, lanes, lane, j);
            eval_ntt51(spectrum, folded);

            for (i = 0; i < (unsigned)kEvalOrder; ++i)
            {
                const unsigned p = j * (unsigned)kEvalOrder + i;
                if (p < n)
                {
                    out[(size_t)p * lanes + lane] = spectrum[i];
                }
            }
        }
    }
    return Rsencode_Success;
}

/*
 * Fold and chirp every coefficient lane for one coset at once.
 *
 * a[r * lanes + l] = c^r * sum_t msg[(r + 51t) * lanes + l] * (c^51)^t.
 *
 * The lane loop must stay innermost: msg rows are lanes wide, so reading a
 * whole row at a time keeps each cache line fully used, whereas a per-lane loop
 * touches one line per coefficient for two bytes of payload.
 */
static void eval_fold_coset_lanes(
    uint16_t *a,
    const uint16_t *msg,
    unsigned d,
    unsigned lanes,
    unsigned j)
{
    const uint16_t *chirp = g_eval_chirplog[j];
    const uint16_t *fold = g_eval_foldlog[j];
    const unsigned tmax = (d + (unsigned)kEvalOrder - 1u) / (unsigned)kEvalOrder;
    unsigned r, t, l;

    for (r = 0; r < (unsigned)kEvalOrder; ++r)
    {
        uint16_t acc[32];

        for (l = 0; l < lanes; ++l)
        {
            acc[l] = 0u;
        }
        for (t = 0; t < tmax; ++t)
        {
            const unsigned idx = r + (unsigned)kEvalOrder * t;
            if (idx < d)
            {
                const uint16_t log_fold = fold[t];
                const uint16_t *row = msg + (size_t)idx * lanes;

                for (l = 0; l < lanes; ++l)
                {
                    acc[l] = gf_add(acc[l], gf_mul_log(row[l], log_fold));
                }
            }
        }
        for (l = 0; l < lanes; ++l)
        {
            a[(size_t)r * lanes + l] = gf_mul_log(acc[l], chirp[r]);
        }
    }
}

static RsencodeResult rs_eval_positions_scalar(
    uint16_t *out,
    const uint16_t *msg,
    unsigned d,
    unsigned lanes,
    const unsigned *pos,
    unsigned count)
{
    unsigned cosets = 0u, j, lane, q, r, nq;
    unsigned qidx[kEvalMaxPositions];

    if (out == NULL || msg == NULL || pos == NULL || lanes == 0u || d == 0u)
    {
        return Rsencode_InvalidInput;
    }
    if (count == 0u)
    {
        return Rsencode_Success;
    }
    if (lanes > 32u || count > (unsigned)kEvalMaxPositions)
    {
        return Rsencode_TooMuchData;
    }
    if ((d + (unsigned)kEvalOrder - 1u) / (unsigned)kEvalOrder > (unsigned)kEvalMaxFold)
    {
        return Rsencode_TooMuchData;
    }
    for (q = 0; q < count; ++q)
    {
        const unsigned c = pos[q] / (unsigned)kEvalOrder + 1u;
        if (c > (unsigned)kEvalMaxCosets)
        {
            return Rsencode_TooMuchData;
        }
        if (c > cosets)
        {
            cosets = c;
        }
    }

    ensure_eval_tables();

    for (j = 0; j < cosets; ++j)
    {
        for (q = 0, nq = 0u; q < count; ++q)
        {
            if (pos[q] / (unsigned)kEvalOrder == j)
            {
                qidx[nq++] = q;
            }
        }
        if (nq == 0u)
        {
            continue;
        }

        if (nq == 1u)
        {
            /*
             * A single opened position does not pay for a fold: evaluate the
             * degree-<d polynomial directly, which costs d products instead of
             * d + |H| + |H|.
             */
            const unsigned q0 = qidx[0];
            const unsigned i = pos[q0] % (unsigned)kEvalOrder;
            const uint16_t lx = (uint16_t)((uint32_t)g_log_table[generator_pow(j)] +
                                           (uint32_t)g_log_table[eval_pow(generator_pow(1285u), i)]);
            uint16_t acc[32];

            for (lane = 0; lane < lanes; ++lane)
            {
                const uint16_t *col = msg + lane;
                acc[lane] = col[(size_t)(d - 1u) * lanes];
            }
            for (r = d - 1u; r-- > 0u;)
            {
                for (lane = 0; lane < lanes; ++lane)
                {
                    acc[lane] = gf_add(gf_mul_log(acc[lane], lx), msg[(size_t)r * lanes + lane]);
                }
            }
            for (lane = 0; lane < lanes; ++lane)
            {
                out[(size_t)q0 * lanes + lane] = acc[lane];
            }
        }
        else
        {
            uint16_t folded[(size_t)kEvalOrder * 32u];

            eval_fold_coset_lanes(folded, msg, d, lanes, j);
            for (q = 0; q < nq; ++q)
            {
                const unsigned q0 = qidx[q];
                const unsigned i = pos[q0] % (unsigned)kEvalOrder;

                for (lane = 0; lane < lanes; ++lane)
                {
                    uint16_t acc = 0u;
                    for (r = 0; r < (unsigned)kEvalOrder; ++r)
                    {
                        acc = gf_add(acc, gf_mul_log(folded[r * lanes + lane], g_eval_wlog[i][r]));
                    }
                    out[(size_t)q0 * lanes + lane] = acc;
                }
            }
        }
    }
    return Rsencode_Success;
}

unsigned rs_eval_coset_order(void)
{
    return (unsigned)kEvalOrder;
}

/* ==========================================================================
 * AVX2 fast path for the 17-point transform.
 *
 * The 17-point butterfly network accounts for 867 of the 935 products per
 * transform, and all of its multipliers are fixed precomputed constants.  That
 * is exactly the case for which a byte-split nibble table with `vpshufb` works:
 * for a fixed multiplier b and a = sum_j nibble_j(a) * x^(4j),
 *
 *     a * b = XOR_j  T_j[nibble_j(a)],      T_j[v] = gf_mul(v * x^(4j), b),
 *
 * so sixteen products cost eight shuffles, six XORs and the nibble extraction,
 * with no table lookup wider than 16 bytes.  Work items (coset, lane) are
 * batched sixteen at a time and the tables for one multiplier are loaded once
 * and reused across every batch.
 * ========================================================================== */
#if defined(__AVX2__)

#include <immintrin.h>

typedef struct
{
    __m256i lo[4];   /* low bytes of T_0..T_3  */
    __m256i hi[4];   /* high bytes of T_0..T_3 */
} gf16_mul_tab;

static gf16_mul_tab g_eval_tab17[17][17];
static int g_eval_tab17_ready = 0;

/* Byte-plane tables for multiplication by the fixed constant b. */
static void gf16_build_tab(gf16_mul_tab *tab, uint16_t b)
{
    unsigned j, v;
    uint8_t lo[16], hi[16];

    for (j = 0; j < 4u; ++j)
    {
        for (v = 0; v < 16u; ++v)
        {
            const uint16_t r = gf_mul((uint16_t)(v << (4u * j)), b);
            lo[v] = (uint8_t)(r & 0xFFu);
            hi[v] = (uint8_t)(r >> 8);
        }
        tab->lo[j] = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i *)(const void *)lo));
        tab->hi[j] = _mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i *)(const void *)hi));
    }
}

/* Sixteen GF(2^16) products by one fixed constant.  T_j[0] = 0, so the padding
 * bytes that vpshufb reads with index 0 contribute nothing. */
static inline __m256i gf16_mul_simd(const gf16_mul_tab *tab, __m256i a)
{
    const __m256i m0f = _mm256_set1_epi16(0x000F);
    const __m256i n0 = _mm256_and_si256(a, m0f);
    const __m256i n1 = _mm256_and_si256(_mm256_srli_epi16(a, 4), m0f);
    const __m256i n2 = _mm256_and_si256(_mm256_srli_epi16(a, 8), m0f);
    const __m256i n3 = _mm256_and_si256(_mm256_srli_epi16(a, 12), m0f);
    const __m256i lo = _mm256_xor_si256(
        _mm256_xor_si256(_mm256_shuffle_epi8(tab->lo[0], n0), _mm256_shuffle_epi8(tab->lo[1], n1)),
        _mm256_xor_si256(_mm256_shuffle_epi8(tab->lo[2], n2), _mm256_shuffle_epi8(tab->lo[3], n3)));
    const __m256i hi = _mm256_xor_si256(
        _mm256_xor_si256(_mm256_shuffle_epi8(tab->hi[0], n0), _mm256_shuffle_epi8(tab->hi[1], n1)),
        _mm256_xor_si256(_mm256_shuffle_epi8(tab->hi[2], n2), _mm256_shuffle_epi8(tab->hi[3], n3)));

    return _mm256_or_si256(lo, _mm256_slli_epi16(hi, 8));
}

static void ensure_eval_tab17(void)
{
    unsigned k, n;
    if (g_eval_tab17_ready)
    {
        return;
    }
    for (k = 0; k < 17u; ++k)
    {
        for (n = 0; n < 17u; ++n)
        {
            gf16_build_tab(&g_eval_tab17[k][n], eval_pow(eval_pow(generator_pow(1285u), 3u), k * n));
        }
    }
    g_eval_tab17_ready = 1;
}

/*
 * Batched evaluation: work item w = coset * lanes + lane.  Phases 1 and 2 are
 * done per work item (they are only 7% of the products), the three 17-point
 * transforms are then run over the whole batch with the tables loaded once per
 * multiplier.
 */
static RsencodeResult rs_eval_codeword_avx2(
    uint16_t *out,
    const uint16_t *msg,
    unsigned d,
    unsigned n,
    unsigned lanes)
{
    const unsigned cosets = (n + (unsigned)kEvalOrder - 1u) / (unsigned)kEvalOrder;
    const unsigned work = cosets * lanes;
    const unsigned wp = (work + 63u) & ~63u;   /* 4 AVX2 vectors per group */
    uint16_t *stage;
    uint16_t *outbuf;
    unsigned coset, lane, n2, k1, k, b;
    RsencodeResult rc = Rsencode_Success;

    ensure_eval_tables();    /* builds the log/exp, chirp, fold and twiddle tables */
    ensure_eval_tab17();

    /*
     * Scratch is grown once and reused; reallocating per call dominates the
     * transform itself at these sizes.  It must be thread-local: the Optimized
     * implementation encodes the two committed vectors concurrently.
     */
    {
        static __thread uint16_t *s_stage = NULL;
        static __thread uint16_t *s_outbuf = NULL;
        static __thread size_t s_wp = 0;

        if (wp > s_wp)
        {
            free(s_stage);
            free(s_outbuf);
            s_stage = (uint16_t *)calloc((size_t)51u * wp, sizeof(uint16_t));
            s_outbuf = (uint16_t *)calloc((size_t)51u * wp, sizeof(uint16_t));
            if (s_stage == NULL || s_outbuf == NULL)
            {
                return Rsencode_Platform;
            }
            s_wp = wp;
        }
        stage = s_stage;
        outbuf = s_outbuf;
    }

    /* phase 1: fold, three-point butterflies and twiddles, per work item */
    for (coset = 0; coset < cosets; ++coset)
    {
        for (lane = 0; lane < lanes; ++lane)
        {
            const unsigned w = coset * lanes + lane;
            uint16_t folded[kEvalOrder];

            eval_fold_coset(folded, msg, d, lanes, lane, coset);
            for (n2 = 0; n2 < 17u; ++n2)
            {
                uint16_t col[3];
                uint16_t res[3];

                col[0] = folded[17u * 0u + n2];
                col[1] = folded[17u * 1u + n2];
                col[2] = folded[17u * 2u + n2];
                eval_dft3(res, col);
                for (k1 = 0; k1 < 3u; ++k1)
                {
                    stage[(size_t)k1 * 17u * wp + (size_t)n2 * wp + w] =
                        gf_mul_log(res[k1], g_eval_twlog[n2][k1]);
                }
            }
        }
    }

    /* phase 2: the three 17-point transforms, batched over work items */
    for (k1 = 0; k1 < 3u; ++k1)
    {
        for (k = 0; k < 17u; ++k)
        {
            const uint16_t *in_k1 = stage + (size_t)k1 * 17u * wp;
            uint16_t *out_k = outbuf + (size_t)(k1 + 3u * k) * wp;

            /* Four accumulators stay in registers across the whole inner loop,
             * so one table load serves 64 products. */
            for (b = 0; b < wp; b += 64u)
            {
                __m256i a0 = _mm256_setzero_si256();
                __m256i a1 = _mm256_setzero_si256();
                __m256i a2 = _mm256_setzero_si256();
                __m256i a3 = _mm256_setzero_si256();

                for (n2 = 0; n2 < 17u; ++n2)
                {
                    const gf16_mul_tab *tab = &g_eval_tab17[k][n2];
                    const uint16_t *col = in_k1 + (size_t)n2 * wp + b;

                    a0 = _mm256_xor_si256(a0, gf16_mul_simd(tab, _mm256_loadu_si256((const __m256i *)(const void *)(col + 0u))));
                    a1 = _mm256_xor_si256(a1, gf16_mul_simd(tab, _mm256_loadu_si256((const __m256i *)(const void *)(col + 16u))));
                    a2 = _mm256_xor_si256(a2, gf16_mul_simd(tab, _mm256_loadu_si256((const __m256i *)(const void *)(col + 32u))));
                    a3 = _mm256_xor_si256(a3, gf16_mul_simd(tab, _mm256_loadu_si256((const __m256i *)(const void *)(col + 48u))));
                }
                _mm256_storeu_si256((__m256i *)(void *)(out_k + b + 0u), a0);
                _mm256_storeu_si256((__m256i *)(void *)(out_k + b + 16u), a1);
                _mm256_storeu_si256((__m256i *)(void *)(out_k + b + 32u), a2);
                _mm256_storeu_si256((__m256i *)(void *)(out_k + b + 48u), a3);
            }
        }
    }

    /* phase 3: scatter the transform results back to codeword positions */
    for (coset = 0; coset < cosets; ++coset)
    {
        for (lane = 0; lane < lanes; ++lane)
        {
            const unsigned w = coset * lanes + lane;
            unsigned i;

            for (i = 0; i < (unsigned)kEvalOrder; ++i)
            {
                const unsigned p = coset * (unsigned)kEvalOrder + i;
                if (p < n)
                {
                    out[(size_t)p * lanes + lane] = outbuf[(size_t)i * wp + w];
                }
            }
        }
    }

    return rc;
}
#endif /* __AVX2__ */


static RsencodeResult rs_eval_codeword_avx2(
    uint16_t *out, const uint16_t *msg, unsigned d, unsigned n, unsigned lanes);

/*
 * The AVX2 path is bit-identical to the scalar one; it is selected when the
 * compiler targets AVX2 and the shape is in range, and the portable scalar path
 * runs otherwise.
 */
RsencodeResult rs_eval_codeword(
    uint16_t *out,
    const uint16_t *msg,
    unsigned d,
    unsigned n,
    unsigned lanes)
{
#if defined(__AVX2__) && !defined(RS_EVAL_NO_SIMD)
    if (out != NULL && msg != NULL && lanes != 0u && d != 0u && n != 0u)
    {
        const unsigned cosets = (n + (unsigned)kEvalOrder - 1u) / (unsigned)kEvalOrder;
        if (cosets <= (unsigned)kEvalMaxCosets &&
            (d + (unsigned)kEvalOrder - 1u) / (unsigned)kEvalOrder <= (unsigned)kEvalMaxFold)
        {
            return rs_eval_codeword_avx2(out, msg, d, n, lanes);
        }
    }
#endif
    return rs_eval_codeword_scalar(out, msg, d, n, lanes);
}

/* ==========================================================================
 * AVX2 fast path for the opened-position evaluator.
 *
 * All three ways a position is produced -- the direct Horner pass, the coset
 * fold, and the omega^(i*r) inner product -- apply one fixed constant to every
 * coefficient lane at once, so the lane axis is a perfect SIMD axis for the
 * nibble-table product above.  The omega^(i*r) tables are precomputed because
 * there are only |H|^2 of them.
 * ========================================================================== */
#if defined(__AVX2__)

#define EVAL_POS_NVEC 2u

static gf16_mul_tab g_eval_wtab[kEvalOrder][kEvalOrder];
static int g_eval_wtab_ready = 0;
static __thread uint16_t *g_eval_pad = NULL;
static __thread size_t g_eval_pad_cap = 0;

static void ensure_eval_wtab(void)
{
    unsigned i, r;

    if (g_eval_wtab_ready)
    {
        return;
    }
    for (i = 0; i < (unsigned)kEvalOrder; ++i)
    {
        for (r = 0; r < (unsigned)kEvalOrder; ++r)
        {
            gf16_build_tab(&g_eval_wtab[i][r], eval_pow(eval_pow(generator_pow(1285u), i), r));
        }
    }
    g_eval_wtab_ready = 1;
}

/* Zero-padded copy of the message so the top vector of the last row is safe. */
static uint16_t *eval_pad_message(const uint16_t *msg, size_t elems)
{
    if (elems + 16u > g_eval_pad_cap)
    {
        free(g_eval_pad);
        g_eval_pad = (uint16_t *)calloc(elems + 16u, sizeof(uint16_t));
        g_eval_pad_cap = (g_eval_pad != NULL) ? elems + 16u : 0u;
    }
    if (g_eval_pad == NULL)
    {
        return NULL;
    }
    memcpy(g_eval_pad, msg, elems * sizeof(uint16_t));
    memset(g_eval_pad + elems, 0, 16u * sizeof(uint16_t));
    return g_eval_pad;
}

static RsencodeResult rs_eval_positions_avx2(
    uint16_t *out,
    const uint16_t *msg,
    unsigned d,
    unsigned lanes,
    const unsigned *pos,
    unsigned count)
{
    const unsigned nvec = (lanes + 15u) / 16u;
    unsigned qidx[kEvalMaxPositions];
    unsigned cosets = 0u, j, q, nq, r, t, v;
    const uint16_t *mp;
    uint16_t *pad;

    for (q = 0; q < count; ++q)
    {
        const unsigned c = pos[q] / (unsigned)kEvalOrder + 1u;
        if (c > cosets)
        {
            cosets = c;
        }
    }

    ensure_eval_tables();
    ensure_eval_wtab();

    pad = eval_pad_message(msg, (size_t)d * lanes);
    if (pad == NULL)
    {
        return Rsencode_Platform;
    }
    mp = pad;

    for (j = 0; j < cosets; ++j)
    {
        for (q = 0u, nq = 0u; q < count; ++q)
        {
            if (pos[q] / (unsigned)kEvalOrder == j)
            {
                qidx[nq++] = q;
            }
        }
        if (nq == 0u)
        {
            continue;
        }

        if (nq == 1u)
        {
            const unsigned q0 = qidx[0];
            const unsigned i = pos[q0] % (unsigned)kEvalOrder;
            gf16_mul_tab tab;
            __m256i acc[EVAL_POS_NVEC];
            const uint16_t *row;

            gf16_build_tab(&tab, gf_mul(generator_pow(j), eval_pow(generator_pow(1285u), i)));

            row = mp + (size_t)(d - 1u) * lanes;
            for (v = 0; v < nvec; ++v)
            {
                acc[v] = _mm256_loadu_si256((const __m256i *)(const void *)(row + 16u * v));
            }
            for (r = d - 1u; r-- > 0u;)
            {
                row = mp + (size_t)r * lanes;
                for (v = 0; v < nvec; ++v)
                {
                    acc[v] = _mm256_xor_si256(gf16_mul_simd(&tab, acc[v]),
                                              _mm256_loadu_si256((const __m256i *)(const void *)(row + 16u * v)));
                }
            }
            for (v = 0; v < nvec; ++v)
            {
                const unsigned base = 16u * v;
                uint16_t tmp[16];
                _mm256_storeu_si256((__m256i *)(void *)tmp, acc[v]);
                for (q = 0; q < lanes - base && q < 16u; ++q)
                {
                    out[(size_t)q0 * lanes + base + q] = tmp[q];
                }
            }
        }
        else
        {
            uint16_t folded[(size_t)kEvalOrder * 32u];
            const unsigned tmax = (d + (unsigned)kEvalOrder - 1u) / (unsigned)kEvalOrder;
            gf16_mul_tab foldtab[kEvalMaxFold];

            /* The table depends on t only, so build it once per coset. */
            for (t = 0; t < tmax; ++t)
            {
                gf16_build_tab(&foldtab[t], g_exp_table[g_eval_foldlog[j][t]]);
            }

            for (r = 0; r < (unsigned)kEvalOrder; ++r)
            {
                __m256i acc[EVAL_POS_NVEC];

                for (v = 0; v < nvec; ++v)
                {
                    acc[v] = _mm256_setzero_si256();
                }
                for (t = 0; t < tmax; ++t)
                {
                    const unsigned idx = r + (unsigned)kEvalOrder * t;

                    if (idx < d)
                    {
                        const uint16_t *row = mp + (size_t)idx * lanes;

                        for (v = 0; v < nvec; ++v)
                        {
                            acc[v] = _mm256_xor_si256(acc[v],
                                gf16_mul_simd(&foldtab[t], _mm256_loadu_si256((const __m256i *)(const void *)(row + 16u * v))));
                        }
                    }
                }
                {
                    uint16_t tmp[32];
                    const uint16_t chirp = g_exp_table[g_eval_chirplog[j][r]];

                    for (v = 0; v < nvec; ++v)
                    {
                        _mm256_storeu_si256((__m256i *)(void *)(tmp + 16u * v), acc[v]);
                    }
                    for (v = 0; v < lanes; ++v)
                    {
                        folded[(size_t)r * lanes + v] = gf_mul(tmp[v], chirp);
                    }
                }
            }

            for (q = 0; q < nq; ++q)
            {
                const unsigned q0 = qidx[q];
                const unsigned i = pos[q0] % (unsigned)kEvalOrder;

                for (v = 0; v < nvec; ++v)
                {
                    __m256i acc = _mm256_setzero_si256();

                    for (r = 0; r < (unsigned)kEvalOrder; ++r)
                    {
                        const uint16_t *row = folded + (size_t)r * lanes;
                        acc = _mm256_xor_si256(acc,
                            gf16_mul_simd(&g_eval_wtab[i][r],
                                          _mm256_loadu_si256((const __m256i *)(const void *)(row + 16u * v))));
                    }
                    {
                        uint16_t tmp[16];
                        _mm256_storeu_si256((__m256i *)(void *)tmp, acc);
                        for (t = 0; t < lanes - 16u * v && t < 16u; ++t)
                        {
                            out[(size_t)q0 * lanes + 16u * v + t] = tmp[t];
                        }
                    }
                }
            }
        }
    }
    return Rsencode_Success;
}
#endif /* __AVX2__ */


static RsencodeResult rs_eval_positions_avx2(
    uint16_t *out, const uint16_t *msg, unsigned d, unsigned lanes,
    const unsigned *pos, unsigned count);

RsencodeResult rs_eval_positions(
    uint16_t *out,
    const uint16_t *msg,
    unsigned d,
    unsigned lanes,
    const unsigned *pos,
    unsigned count)
{
#if defined(__AVX2__) && !defined(RS_EVAL_NO_SIMD)
    if (out != NULL && msg != NULL && pos != NULL && lanes != 0u && d != 0u && count != 0u &&
        lanes <= 32u && count <= (unsigned)kEvalMaxPositions &&
        (d + (unsigned)kEvalOrder - 1u) / (unsigned)kEvalOrder <= (unsigned)kEvalMaxFold)
    {
        /*
         * The omega^(i*r) nibble tables are built once per process, so this path
         * is a strict win from the second verification on for SIG256/SIG512 and
         * from the tenth for SIG128.  Both paths agree bit for bit, so the
         * choice only affects timing.
         */
        return rs_eval_positions_avx2(out, msg, d, lanes, pos, count);
    }
#endif
    return rs_eval_positions_scalar(out, msg, d, lanes, pos, count);
}
