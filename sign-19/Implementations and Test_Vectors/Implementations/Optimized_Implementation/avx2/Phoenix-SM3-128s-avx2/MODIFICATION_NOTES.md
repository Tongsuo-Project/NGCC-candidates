# Phoenix-SM3-AVX2 修改说明

本文档说明当前 `Phoenix-SM3-*/` AVX2 优化目录中相对于最早从 GitHub 下载的
Phoenix 参考实现做了哪些修改、修改前后内容是什么、为什么修改，以及如何复现
和检查这些修改。

文中以 `ref/...` 表示原始仓库中的对应文件；在本独立包中，对应文件位于
`Phoenix-SM3-*/...`。

## 1. 对比基线

- 原始仓库：`https://github.com/Love-MaShiro/phoenix`
- 原始分支：`para_test`
- 本地原始提交：`f173cbc`，提交信息为 `默认版本`
- 原始目录：`ref/`
- 当前提交目录：`Phoenix-SM3-*/`
- 当前参数组：`phoenix-sm3-128f/s、phoenix-sm3-192f/s 等`
- 当前说明范围：SM3 AVX2 8路并行优化实现

## 2. 总体修改概览

1. SM3 AVX2 8路并行实现：新增 `sm3_avx2.c`，实现 SM3 的 8路并行计算。
2. 新增并行封装接口：`sm3_x8.c` 提供统一的 `sm3x8()` 接口。
3. 新增并行哈希函数：`hash_sm3x8.c` 实现 8路并行的 `prf_addrx8()`。
4. 新增并行树哈希：`thash_sm3_simplex8.c` 实现 8路并行的 `thashx8()`。
5. 新增并行工具函数：`utilsx8.c` 实现并行链计算、叶子节点生成和树哈希。
6. 所有 SM3 参数组的 `SPX_BYTES` 增加 2 字节，用于实际签名格式中保存
   TFORS 签名长度字段。
7. `sign.c` 做了安全性修正：动态消息缓冲区、TFORS 长度校验、签名总长度
   校验、counter 溢出检查、`crypto_sign_open()` 边界检查。
8. 不再保留 robust thash 构建路径，Makefile 固定使用 simple thash。
9. 增加提交/KAT 构建路径和 DRNG 随机数适配。

## 3. SM3 AVX2 优化相关修改

### 3.1 新增文件列表

AVX2 优化涉及以下新增文件：

```
sm3_avx2.c          # SM3 AVX2 8路并行核心实现
sm3_x8.c            # 8路并行封装接口
sm3_x4.c            # 4路并行封装接口（ARM NEON 预留）
sm3_neon.c          # ARM NEON 4路并行实现
sm3_internal.h      # 内部函数声明
hash_sm3x8.c        # 8路并行 prf_addr 实现
thash_sm3_simplex8.c # 8路并行 thash 实现
utilsx8.c           # 8路并行工具函数
```

### 3.2 `sm3_avx2.c` - AVX2 8路并行核心

这是 SM3 AVX2 优化的核心实现，提供了 `sm3x8_avx2()` 函数。

关键技术：

- 使用 AVX2 256位向量，同时处理 8 个 32 位状态
- 使用 `_mm256_loadu_si256()` 和 `_mm256_storeu_si256()` 进行向量加载/存储
- 使用 `_mm256_xor_si256()` 进行并行异或
- 使用 `_mm256_rotl_epi32()` 进行并行旋转

```c
// 核心函数声明
void sm3x8_avx2(uint8_t *out0, uint8_t *out1, ..., uint8_t *out7,
                 const uint8_t *in0, const uint8_t *in1, ...,
                 size_t inlen);
```

### 3.3 `sm3_x8.c` - 8路并行封装接口

提供统一的 `sm3x8()` 接口，根据编译时宏定义选择实现：

```c
#if SM3_ENABLE_AVX2
    sm3x8_avx2(...);  // 使用 AVX2 加速
#else
    // 降级到标量实现
    sm3(out0, in0, inlen);
    ...
#endif
```

### 3.4 `hash_sm3x8.c` - 8路并行 PRF

实现 8路并行的 `prf_addrx8()` 函数，用于并行计算多个地址的伪随机输出：

```c
void prf_addrx8(unsigned char *out0, ..., unsigned char *out7,
                 const spx_ctx *ctx, const uint32_t addrx8[8 * 8]);
```

### 3.5 `thash_sm3_simplex8.c` - 8路并行树哈希

实现 8路并行的 `thashx8()` 函数，用于并行计算多个叶子节点的哈希：

```c
void thashx8(unsigned char *out0, ..., unsigned char *out7,
              const unsigned char *in0, ..., const unsigned char *in7,
              unsigned int inblocks,
              const spx_ctx *ctx, const uint32_t addrx8[8 * 8]);
```

### 3.6 `utilsx8.c` - 8路并行工具函数

提供并行链计算、叶子节点生成和树哈希功能：

```c
void gen_leafx8(unsigned char *leafx8[8 * SPX_N],
                const spx_ctx *ctx,
                const unsigned char *sk_seed,
                const unsigned char *pub_seed,
                uint32_t leaf_idxx8[8],
                uint32_t *auth_path);
```

## 4. SM3 参数组签名长度修正

涉及文件：

```text
ref/params/params-phoenix-sm3-128f.h
ref/params/params-phoenix-sm3-128s.h
ref/params/params-phoenix-sm3-192f.h
ref/params/params-phoenix-sm3-192s.h
ref/params/params-phoenix-sm3-256f.h
ref/params/params-phoenix-sm3-256s.h
ref/params/params-phoenix-sm3-384f.h
ref/params/params-phoenix-sm3-384s.h
ref/params/params-phoenix-sm3-512f.h
ref/params/params-phoenix-sm3-512s.h
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

- `sign.c` 中签名格式实际包含 2 字节 TFORS 签名长度字段
- 原始 SM3 参数头的 `SPX_BYTES` 没有把这 2 字节计入最大签名长度

## 5. `sign.c` 安全性和健壮性修改

这些修改是公共签名逻辑，SM3 版本同样使用。详见 SHAKE 版本的 `MODIFICATION_NOTES.md` 第 5 节。

## 6. Makefile AVX2 配置

### 6.1 编译选项

```makefile
AVX2_CFLAGS = -mavx2
CFLAGS += $(AVX2_CFLAGS) -DSM3_ENABLE_AVX2 -DAVX2_OPTIMIZED
```

### 6.2 源文件配置

```makefile
CORE_SOURCES = \
    ...
    hash_sm3x8.c \
    thash_sm3_simple.c thash_sm3_simplex8.c \
    utilsx1.c utilsx8.c \
    ...

AVX2_SOURCES = \
    sm3_avx2.c \
    sm3_x4.c \
    sm3_x8.c
```

## 7. 如何复现这些修改

从干净原始仓库开始：

```sh
git clone https://github.com/Love-MaShiro/phoenix.git
cd phoenix
git checkout para_test
```

### 7.1 添加 SM3 AVX2 优化文件

创建以下新文件：

- `ref/sm3_avx2.c`
- `ref/sm3_x8.c`
- `ref/sm3_x4.c`
- `ref/sm3_neon.c`
- `ref/sm3_internal.h`
- `ref/hash_sm3x8.c`
- `ref/thash_sm3_simplex8.c`
- `ref/utilsx8.c`

### 7.2 修改 SM3 参数头

在所有 `ref/params/params-phoenix-sm3-*.h` 文件中：

```c
// 修改前
#define SPX_BYTES (...)

// 修改后
#define SPX_BYTES (... + 2)
```

### 7.3 修改 Makefile

添加 AVX2 编译选项和源文件配置。

### 7.4 删除 robust thash 文件

删除以下文件（如果存在）：

```text
ref/thash_sm3_robust.c
```

## 8. 性能验证

### 8.1 编译测试

```sh
cd Phoenix-SM3-128f-avx2
make clean && make
./KAT_SIG
```

### 8.2 性能对比

在支持 AVX2 的 x86 平台上，SM3 AVX2 8路并行实现相比标量实现可达到 **4-5x** 的加速比：

| 输入长度 | 标量时间 | AVX2 时间 | 加速比 |
|---------|---------|----------|--------|
| 64B | 74.29ms | 14.15ms | 5.25x |
| 128B | 43.22ms | 9.32ms | 4.64x |
| 256B | 35.84ms | 7.47ms | 4.80x |
| 512B | 32.10ms | 6.58ms | 4.88x |

### 8.3 KAT 测试

```sh
make kat
# 期望输出文件：output/KAT_SIG_Phoenix-SM3-*.txt
```

## 9. 检查命令

### 确认 AVX2 优化文件存在

```sh
ls -la Phoenix-SM3-*-avx2/sm3_avx2.c
ls -la Phoenix-SM3-*-avx2/sm3_x8.c
ls -la Phoenix-SM3-*-avx2/hash_sm3x8.c
```

### 确认编译选项

```sh
grep "SM3_ENABLE_AVX2" Phoenix-SM3-*-avx2/Makefile
grep "mavx2" Phoenix-SM3-*-avx2/Makefile
```

### 确认 SM3 参数组包含 +2

```sh
grep "SPX_BYTES.*+ 2" ref/params/params-phoenix-sm3-*.h
```

## 10. 文件结构

```
Phoenix-SM3-128f-avx2/
├── README.md              # 基本说明
├── MODIFICATION_NOTES.md   # 本文档
├── Makefile               # 构建配置（包含 AVX2 选项）
├── sm3.h                  # SM3 公共 API
├── sm3_internal.h         # SM3 内部 API（AVX2/NEON）
├── sm3_scalar.c           # SM3 标量实现
├── sm3_avx2.c             # SM3 AVX2 8路并行实现
├── sm3_x8.c               # 8路并行封装接口
├── sm3_x4.c               # 4路并行封装接口
├── sm3_neon.c             # ARM NEON 4路并行实现
├── hash_sm3.c             # SM3 哈希函数
├── hash_sm3x8.c           # 8路并行 prf_addr
├── thash_sm3_simple.c     # 简单 thash
├── thash_sm3_simplex8.c   # 8路并行 thash
├── utils.c                # 工具函数
├── utilsx1.c              # 单路工具函数
├── utilsx8.c              # 8路并行工具函数
└── ...
```
