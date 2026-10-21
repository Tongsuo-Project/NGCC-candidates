# Phoenix-SHAKE-128s 修改说明

本文档说明当前 `Phoenix-SHAKE-128s/` 独立提交目录中的 SHAKE-128s 版本
相对于最早从 GitHub 下载的 Phoenix 参考实现做了哪些修改、修改前后内容
是什么、为什么修改，以及如何复现和检查这些修改。

文中以 `ref/...` 表示原始仓库中的对应文件；在本独立包中，对应文件位于
`Phoenix-SHAKE-128s/...`。

## 1. 对比基线

- 原始仓库：`https://github.com/Love-MaShiro/phoenix`
- 原始分支：`para_test`
- 本地原始提交：`f173cbc`，提交信息为 `默认版本`
- 原始目录：`ref/`
- 当前提交目录：`Phoenix-SHAKE-128s/`
- 当前参数组：`phoenix-shake-128s`
- 当前说明范围：SHAKE-128s 独立提交实现，并说明所有 SHAKE 参数头中的
  公共修正来源。

## 2. 总体修改概览

1. SHAKE 版本增加 `pub_seed` 预吸收状态缓存，减少重复 SHAKE 吸收开销。
2. `hash_shake.c` 中的 `prf_addr()` 改为复用预吸收状态。
3. `thash_shake_simple.c` 中的 `thash()` 改为复用预吸收状态。
4. 所有 SHAKE 参数组的 `SPX_BYTES` 增加 2 字节，用于实际签名格式中保存
   TFORS 签名长度字段。
5. `sign.c` 做了安全性修正：动态消息缓冲区、TFORS 长度校验、签名总长度
   校验、counter 溢出检查、`crypto_sign_open()` 边界检查。
6. 不再保留 robust thash 构建路径，Makefile 固定使用 simple thash。
7. 增加提交/KAT 构建路径和 DRNG 随机数适配。

## 3. SHAKE 性能相关修改

### 3.1 `context.h`

当前位置：

- `ref/context.h` 第 27-30 行

修改前：

```c
#ifdef SPX_SM3
    // sm3 state that absorbed pub_seed
    uint8_t state_seeded_sm3[40];
#endif

// Always include these to allow Haraka unit tests even when not the primary hash
uint64_t tweaked512_rc64[10][8];
uint32_t tweaked256_rc32[10][8];
```

修改后：

```c
#ifdef SPX_SM3
    // sm3 state that absorbed pub_seed
    uint8_t state_seeded_sm3[40];
#endif

#ifdef SPX_SHAKE
    // shake256 state that absorbed pub_seed
    uint64_t state_seeded_shake[26];
#endif

// Always include these to allow Haraka unit tests even when not the primary hash
uint64_t tweaked512_rc64[10][8];
uint32_t tweaked256_rc32[10][8];
```

修改原因：

- SHAKE 的 `prf_addr()` 和 `thash()` 都反复使用 `pub_seed` 作为输入前缀。
- 原始实现每次都重新拼接并从头吸收 `pub_seed`。
- 新增 `state_seeded_shake[26]` 后，可以在每次初始化 hash context 时先吸收
  `pub_seed`，后续调用复制该状态并继续吸收地址和输入。
- 这属于性能优化，不改变输入域和输出结果。

### 3.2 `hash_shake.c`

当前位置：

- `ref/hash_shake.c` 第 11-15 行：初始化预吸收状态。
- `ref/hash_shake.c` 第 20-30 行：`prf_addr()` 复用预吸收状态。

#### `initialize_hash_function()`

修改前：

```c
/* For SHAKE256, there is no immediate reason to initialize at the start,
   so this function is an empty operation. */
void initialize_hash_function(spx_ctx* ctx)
{
    (void)ctx; /* Suppress an 'unused parameter' warning. */
}
```

修改后：

```c
void initialize_hash_function(spx_ctx* ctx)
{
    shake256_inc_init(ctx->state_seeded_shake);
    shake256_inc_absorb(ctx->state_seeded_shake, ctx->pub_seed, SPX_N);
}
```

修改原因：

- 原来 SHAKE 初始化是空操作。
- 现在在 `spx_ctx` 中缓存已经吸收 `pub_seed` 的 SHAKE 状态，供
  `prf_addr()` 和 `thash()` 复用。

#### `prf_addr()`

修改前：

```c
void prf_addr(unsigned char *out, const spx_ctx *ctx,
              const uint32_t addr[8])
{
    unsigned char buf[2*SPX_N + SPX_ADDR_BYTES];

    memcpy(buf, ctx->pub_seed, SPX_N);
    memcpy(buf + SPX_N, addr, SPX_ADDR_BYTES);
    memcpy(buf + SPX_N + SPX_ADDR_BYTES, ctx->sk_seed, SPX_N);

    shake256(out, SPX_N, buf, 2*SPX_N + SPX_ADDR_BYTES);
}
```

修改后：

```c
void prf_addr(unsigned char *out, const spx_ctx *ctx,
              const uint32_t addr[8])
{
    uint64_t state[26];

    memcpy(state, ctx->state_seeded_shake, sizeof(state));
    shake256_inc_absorb(state, (const unsigned char *)addr, SPX_ADDR_BYTES);
    shake256_inc_absorb(state, ctx->sk_seed, SPX_N);
    shake256_inc_finalize(state);
    shake256_inc_squeeze(out, SPX_N, state);
}
```

修改原因：

- 修改前每次 `prf_addr()` 都构造
  `pub_seed || addr || sk_seed` 并调用 one-shot `shake256()`。
- 修改后复用已吸收 `pub_seed` 的状态，只继续吸收 `addr || sk_seed`。
- 等价于原来的输入顺序，减少重复吸收 `pub_seed` 的成本。

未修改的 SHAKE 路径：

- `gen_message_random()` 仍然对 `sk_prf || optrand || m` 使用 SHAKE 增量吸收。
- `hash_message()` 仍然对 `R || pk || m` 使用 SHAKE 增量吸收。

这两处没有公共 `pub_seed` 前缀可复用，因此不做预吸收优化。

### 3.3 `thash_shake_simple.c`

当前位置：

- `ref/thash_shake_simple.c` 第 14-24 行。

修改前：

```c
void thash(unsigned char *out, const unsigned char *in, unsigned int inblocks,
           const spx_ctx *ctx, uint32_t addr[8])
{
    SPX_VLA(uint8_t, buf, SPX_N + SPX_ADDR_BYTES + inblocks*SPX_N);

    memcpy(buf, ctx->pub_seed, SPX_N);
    memcpy(buf + SPX_N, addr, SPX_ADDR_BYTES);
    memcpy(buf + SPX_N + SPX_ADDR_BYTES, in, inblocks * SPX_N);

    shake256(out, SPX_N, buf, SPX_N + SPX_ADDR_BYTES + inblocks*SPX_N);
}
```

修改后：

```c
void thash(unsigned char *out, const unsigned char *in, unsigned int inblocks,
           const spx_ctx *ctx, uint32_t addr[8])
{
    uint64_t state[26];

    memcpy(state, ctx->state_seeded_shake, sizeof(state));
    shake256_inc_absorb(state, (const unsigned char *)addr, SPX_ADDR_BYTES);
    shake256_inc_absorb(state, in, inblocks * SPX_N);
    shake256_inc_finalize(state);
    shake256_inc_squeeze(out, SPX_N, state);
}
```

修改原因：

- `thash()` 在 GWOTS、TFORS、Merkle tree 中调用次数非常多。
- 原始实现每次都从 `pub_seed` 开始构造完整输入并 one-shot `shake256()`。
- 修改后复用 `state_seeded_shake`，避免重复吸收 `pub_seed`。
- 输入语义仍等价于
  `SHAKE256(pub_seed || addr || in, SPX_N)`。

`thash_init_bitmask()` 和 `thash_fin()`：

- simple SHAKE 模式下 bitmask 不使用，因此 `thash_init_bitmask()` 仍为空操作。
- `thash_fin()` 仍直接调用 `thash()`。

## 4. SHAKE 参数组签名长度修正

涉及文件：

```text
ref/params/params-phoenix-shake-128f.h
ref/params/params-phoenix-shake-128s.h
ref/params/params-phoenix-shake-192f.h
ref/params/params-phoenix-shake-192s.h
ref/params/params-phoenix-shake-256f.h
ref/params/params-phoenix-shake-256s.h
ref/params/params-phoenix-shake-384f.h
ref/params/params-phoenix-shake-384s.h
ref/params/params-phoenix-shake-512f.h
ref/params/params-phoenix-shake-512s.h
```

修改前：

```c
#define SPX_BYTES (SPX_N + COUNTER_SIZE + SPX_TFORS_SIG_MAX + SPX_D * (SPX_GWOTS_BYTES + COUNTER_SIZE) + \
                   SPX_FULL_HEIGHT * SPX_N)
```

修改后：

```c
#define SPX_BYTES (SPX_N + COUNTER_SIZE + 2 + SPX_TFORS_SIG_MAX + SPX_D * (SPX_GWOTS_BYTES + COUNTER_SIZE) + \
                   SPX_FULL_HEIGHT * SPX_N)
```

修改原因：

- `sign.c` 中签名格式实际包含 2 字节 TFORS 签名长度字段：

```c
ull_to_bytes(sig, 2, tfors_siglen);
sig += 2;
```

- 原始 SHAKE 参数头的 `SPX_BYTES` 没有把这 2 字节计入最大签名长度。
- 若外部按 `CRYPTO_BYTES` 分配签名缓冲区，可能出现长度不足。
- 因此所有 SHAKE 参数组统一补上 `+ 2`。

以 `params-phoenix-shake-128s.h` 为例，当前位置是第 99-100 行。

## 5. `sign.c` 安全性和健壮性修改

这些修改是公共签名逻辑，SHAKE 版本同样使用。

### 5.1 固定消息缓冲区改为动态分配

修改前：

```c
#define SPX_MAX_MLEN 3300
...
unsigned char mtmp[SPX_MAX_MLEN  + 4];
...
memcpy(mtmp, m, mlen);
```

修改后：

```c
static unsigned char *alloc_message_buffer(const unsigned char *m, size_t mlen)
{
    unsigned char *mtmp;

    if (mlen > SIZE_MAX - COUNTER_SIZE) {
        return NULL;
    }

    mtmp = malloc(mlen + COUNTER_SIZE);
    if (mtmp == NULL) {
        return NULL;
    }

    memcpy(mtmp, m, mlen);
    return mtmp;
}
```

当前位置：

- `ref/sign.c` 第 20-35 行。
- 签名侧使用位置：第 167-173 行。
- 验签侧使用位置：第 282-287 行。

修改原因：

- 原始 `SPX_MAX_MLEN = 3300` 是固定上限。
- 当消息超过该长度时，`memcpy(mtmp, m, mlen)` 有越界风险。
- 动态分配 `mlen + COUNTER_SIZE` 后，消息长度不再受硬编码栈数组限制。

### 5.2 签名长度计算增加溢出检查

修改前：

```c
*siglen = SPX_N + COUNTER_SIZE + 2 + tfors_siglen + SPX_D * (SPX_GWOTS_BYTES + SPX_TREE_HEIGHT * SPX_N + COUNTER_SIZE);
```

修改后：

```c
static int signature_length_from_tfors(size_t tfors_siglen, size_t *siglen)
{
    const size_t gwots_layer_len = SPX_GWOTS_BYTES + SPX_TREE_HEIGHT * SPX_N + COUNTER_SIZE;
    const size_t fixed_siglen = SPX_N + COUNTER_SIZE + 2 + (size_t)SPX_D * gwots_layer_len;

    if (tfors_siglen > SIZE_MAX - fixed_siglen) {
        return -1;
    }

    *siglen = fixed_siglen + tfors_siglen;
    return 0;
}
```

当前位置：

- helper：`ref/sign.c` 第 37-48 行。
- 签名侧调用：第 228-234 行。

修改原因：

- 明确把签名固定部分和可变 TFORS 部分分开计算。
- 对 `size_t` 加法做溢出检查。
- 与第 4 节的 `SPX_BYTES + 2` 修正保持一致。

### 5.3 TFORS 签名长度校验

修改前：

```c
size_t tfors_siglen = (size_t)bytes_to_ull(sig, 2);
sig += 2;
```

修改后：

```c
tfors_siglen = (size_t)bytes_to_ull(sig, 2);
if (!tfors_siglen_is_valid(tfors_siglen) ||
    signature_length_from_tfors(tfors_siglen, &expected_siglen) != 0 ||
    siglen != expected_siglen) {
    free(mtmp);
    return -1;
}
sig += 2;
```

并进一步校验：

```c
message_to_indices(indices, mhash, &ctx);
octopus_compute_auth_count(indices, SPX_TFORS_K, &tfors_auth_count);
expected_tfors_siglen = SPX_TFORS_K * SPX_N + (size_t)tfors_auth_count * SPX_N;
if (tfors_siglen != expected_tfors_siglen) {
    free(mtmp);
    return -1;
}
```

当前位置：

- `tfors_siglen_is_valid()`：`ref/sign.c` 第 50-55 行。
- 验签侧校验：第 294-310 行。

修改原因：

- 原始验签逻辑直接信任签名中编码的 2 字节 TFORS 长度。
- 畸形签名可能让解析指针跳到错误位置，导致越界读取或错误验签路径。
- 现在会校验：
  1. TFORS 长度是否在合法范围内；
  2. 总签名长度是否与编码长度一致；
  3. 根据消息派生出的 indices 重新计算期望 TFORS 长度，必须一致。

### 5.4 counter 溢出检查

修改前：

```c
for (counter = 0; ; counter++) {
    ...
    if (tfors_siglen <= tfors_sig_max) {
        break;
    }
}
```

修改后：

```c
for (counter = 0; ; counter++) {
    ...
    if (tfors_siglen <= tfors_sig_max) {
        break;
    }
    if (counter == UINT32_MAX) {
        free(mtmp);
        *siglen = 0;
        *tfslen = 0;
        return -1;
    }
}
```

当前位置：

- `ref/sign.c` 第 178-198 行。

修改原因：

- 原始无限循环依赖最终找到满足长度条件的 counter。
- 增加 `UINT32_MAX` 检查，避免极端情况下 counter 回绕。

### 5.5 `crypto_sign_open()` 边界检查

修改前：

```c
unsigned long long slen_tmp = *slen;
unsigned long long tfslen_tmp = *tfslen;
unsigned long long mlen_tmp = smlen - slen_tmp;

if (crypto_sign_verify(sm, (size_t)slen_tmp, sm + slen_tmp, (size_t)mlen_tmp, pk)) {
    memset(m, 0, smlen);
    *mlen = 0;
    return -1;
}
```

修改后：

```c
unsigned long long slen_tmp = *slen;
unsigned long long mlen_tmp;

(void)tfslen;

if (slen_tmp > smlen) {
    *mlen = 0;
    return -1;
}

mlen_tmp = smlen - slen_tmp;

if (crypto_sign_verify(sm, (size_t)slen_tmp, sm + slen_tmp, (size_t)mlen_tmp, pk)) {
    memset(m, 0, (size_t)mlen_tmp);
    *mlen = 0;
    return -1;
}
```

当前位置：

- `ref/sign.c` 第 390-410 行。

修改原因：

- 原始代码在 `slen_tmp > smlen` 时会发生无符号下溢。
- 失败时清零 `smlen` 字节也可能超过消息输出区实际长度。
- 修改后先检查长度关系，只清零消息部分。

## 6. robust thash 删除和 simple thash 固定化

### 6.1 `Makefile`

修改前：

```make
PARAMS = phoenix-sm3-384f
THASH = simple
...
ifneq (,$(findstring shake,$(PARAMS)))
    SOURCES += fips202.c hash_shake.c thash_shake_$(THASH).c
    HEADERS += fips202.h
endif
```

修改后：

```make
PARAMS = phoenix-sm3-128s
...
ifneq (,$(findstring shake,$(PARAMS)))
    SOURCES += fips202.c hash_shake.c thash_shake_simple.c
    HEADERS += fips202.h
endif
```

当前位置：

- `ref/Makefile` 第 1 行、第 9-11 行。

修改原因：

- 当前不需要 robust 版本。
- 固定 `thash_shake_simple.c` 后，避免构建脚本引用已经删除的 robust 文件。
- 对 SHAKE 版本而言，实际使用的就是 simple thash。

### 6.2 删除文件

删除的 robust 文件：

```text
ref/thash_shake_robust.c
ref/thash_sha2_robust.c
ref/thash_sm3_robust.c
ref/thash_haraka_robust.c
```

其中 SHAKE 相关的是：

```text
ref/thash_shake_robust.c
```

修改前：

- 文件存在，并可通过 `THASH=robust` 或 `thash_shake_$(THASH).c` 构建路径引用。

修改后：

- 文件删除。
- Makefile 不再使用 `THASH` 变量选择 robust/simple，而是固定 simple。

修改原因：

- 用户明确不需要 robust 版本。
- 删除无用实现和引用，减少提交/维护范围。

## 7. 提交/KAT 构建与随机数适配

这些修改不是 SHAKE 哈希函数本身的变化，但会影响 SHAKE 版本如何构建
提交 KAT。

### 7.1 `Makefile` 增加 `KAT_SIG`

修改前：

```make
.PHONY: clean test benchmark

default: PQCgenKAT_sign

all: PQCgenKAT_sign tests benchmarks
```

修改后：

```make
.PHONY: clean test benchmark submission

default: KAT_SIG

all: KAT_SIG PQCgenKAT_sign tests benchmarks

submission: KAT_SIG

KAT_SIG: KAT_SIG.c SIG_AlgorithmInstance.c SIG_AlgorithmInstance.h drng.c drng.h $(SOURCES) $(HEADERS)
    $(CC) $(CFLAGS) -DSUBMISSION_DRNG -o $@ $(SOURCES) SIG_AlgorithmInstance.c drng.c KAT_SIG.c $(LDLIBS)
```

当前位置：

- `ref/Makefile` 第 43-60 行。

修改原因：

- 增加提交模板 KAT 目标。
- 编译 `KAT_SIG` 时定义 `SUBMISSION_DRNG`，使随机数来自模板 DRNG。

### 7.2 `randombytes.c`

修改前：

```c
#include <fcntl.h>
#include <unistd.h>

#include "randombytes.h"

static int fd = -1;

void randombytes(unsigned char *x, unsigned long long xlen)
{
    ...
    fd = open("/dev/urandom", O_RDONLY);
    ...
}
```

修改后：

```c
#ifdef SUBMISSION_DRNG

#include <limits.h>
#include <stdlib.h>

#include "drng.h"
#include "randombytes.h"

extern DRNG_ctx drng_algorithm;

void randombytes(unsigned char *x, unsigned long long xlen)
{
    const unsigned long long max_chunk = ULLONG_MAX / 8;

    while (xlen > 0) {
        unsigned long long chunk = xlen < max_chunk ? xlen : max_chunk;

        if (get_random_number(&drng_algorithm, x, chunk * 8) != 0) {
            abort();
        }

        x += chunk;
        xlen -= chunk;
    }
}

#else
...
#endif
```

当前位置：

- `ref/randombytes.c` 第 5-31 行。

修改原因：

- 原始实现从 `/dev/urandom` 读取随机数。
- 提交 KAT 需要确定性随机源，因此在 `SUBMISSION_DRNG` 下改用提交模板 DRNG。
- 未定义 `SUBMISSION_DRNG` 时仍保留原始 `/dev/urandom` 行为。

## 8. benchmark 脚本调整

### `run_all_variants.sh`

修改前：

```bash
MODES=("simple")
...
make tests benchmarks PARAMS=$PARAM THASH=$mode
...
echo "Variant: $PARAM ($mode)"
```

修改后：

```bash
PARAM="phoenix-${hash}-${level}${var}"
...
make tests benchmarks PARAMS=$PARAM
...
echo "Variant: $PARAM"
```

当前位置：

- `ref/run_all_variants.sh` 第 20-80 行。

修改原因：

- Makefile 已经固定 simple thash，不再需要 `THASH`/`MODES`。
- benchmark 输出也不再带 `(simple)` 后缀。

## 9. README 修改

修改前：

```text
PARAMS: 选择参数集（例如 sphincs-haraka-128f, sphincs-shake-128s 等）
THASH: 选择哈希模式（robust 或 simple）
make all PARAMS=sphincs-shake-128f THASH=simple
```

修改后：

```text
PARAMS: 选择参数集（例如 phoenix-haraka-128f, phoenix-shake-128s 等）
make all PARAMS=phoenix-shake-128f
```

修改原因：

- 参数组名称应为 Phoenix 当前命名。
- robust/simple 模式不再通过 `THASH` 暴露。

## 10. 如何复现这些修改

从干净原始仓库开始：

```sh
git clone https://github.com/Love-MaShiro/phoenix.git
cd phoenix
git checkout para_test
```

按本文第 3-9 节修改以下文件：

```text
ref/context.h
ref/hash_shake.c
ref/thash_shake_simple.c
ref/sign.c
ref/Makefile
ref/randombytes.c
ref/run_all_variants.sh
ref/README.md
ref/params/params-phoenix-shake-128f.h
ref/params/params-phoenix-shake-128s.h
ref/params/params-phoenix-shake-192f.h
ref/params/params-phoenix-shake-192s.h
ref/params/params-phoenix-shake-256f.h
ref/params/params-phoenix-shake-256s.h
ref/params/params-phoenix-shake-384f.h
ref/params/params-phoenix-shake-384s.h
ref/params/params-phoenix-shake-512f.h
ref/params/params-phoenix-shake-512s.h
```

并删除：

```text
ref/thash_shake_robust.c
ref/thash_sha2_robust.c
ref/thash_sm3_robust.c
ref/thash_haraka_robust.c
```

如果需要提交 KAT 目标，还需要加入提交模板文件：

```text
ref/KAT_SIG.c
ref/SIG_AlgorithmInstance.c
ref/SIG_AlgorithmInstance.h
ref/drng.c
ref/drng.h
```

## 11. 检查命令

### 查看 SHAKE 核心差异

```sh
git diff -- ref/context.h ref/hash_shake.c ref/thash_shake_simple.c
```

### 确认 SHAKE 参数组都包含 `+ 2`

```sh
rg -n "SPX_BYTES .*\\+ 2" ref/params/params-phoenix-shake-*.h
```

### 确认 robust 文件已删除或不再参与构建

```sh
git status --short -- \
  ref/thash_shake_robust.c \
  ref/thash_sha2_robust.c \
  ref/thash_sm3_robust.c \
  ref/thash_haraka_robust.c
rg -n "THASH|thash_shake_\\$\\(THASH\\)" ref/Makefile ref/run_all_variants.sh
```

期望：

- `ref/thash_*_robust.c` 显示删除或不存在。
- Makefile 和 benchmark 脚本不再通过 `THASH` 拼接 robust/simple 文件名。

### 构建 SHAKE 128s

```sh
cd ref
make clean
make test PARAMS=phoenix-shake-128s
```

或者只构建 benchmark：

```sh
make benchmark PARAMS=phoenix-shake-128s
```
