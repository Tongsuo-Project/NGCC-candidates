# BiT-512（参考实现）

## 参数

| N | Q | (K,L) | τ/β | γ1/γ2/γ_B | ‖h‖∞ |
|------|--------|-------|---------|---------------|------|
| 1024 | 520193 | (3,3) | 115/115 | 8192/69481/64 | 1 |

公钥 5056，私钥 9024，签名 6695（字节）。

## 编译

```sh
make             # 生成 KAT_SIG
make correctness # 正确性测试
make speed       # 性能测试
make clean
```

`./KAT_SIG` 在 `output/` 下生成 `KAT_SIG_BiT-512.txt`，含 10 组向量。

文件说明见上级目录的 `README.md`。

## 说明

- 哈希后端由 `symmetric.h` 的 `BIT_USE_SHAKE` 选择：0 为 SM3（默认），1 为 SHAKE。
- hint 的 ‖h‖∞ = 1，按 base-3 编码，每 5 个三元系数压入 1 字节（3⁵=243≤256，`pack_poly_h`；解码用 `h3_divmod` 做逐位 /3）。
- b1、z0、z1、b0 用通用比特打包器 `pack_bits` / `unpack_bits`。
