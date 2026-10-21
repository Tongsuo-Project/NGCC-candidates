CTL KEM 自评估代码使用说明（MinGW）

文件：
1. benchmark_selfeval_ctl.c
   用于性能、资源消耗、传输与存储开销测试。
   输出：
   - ctl_kem_benchmark_raw.csv
   - ctl_kem_benchmark_summary.csv

2. test_kem_correctness_ctl.c
   只用于正确性测试。
   输出：
   - ctl_kem_kat_vectors.txt

编译示例：
请把下面命令中的 CTL_SOURCE_FILES 替换成你的算法实现源文件，例如：
KEM_CTL-257-512.c ctl.c drng.c randombytes.c ...

正确性测试：
gcc -std=c99 -O2 -Wall -Wextra -Wpedantic test_kem_correctness_ctl.c CTL_SOURCE_FILES -o test_kem.exe

自评估测试：
gcc -std=c99 -O2 -Wall -Wextra -Wpedantic benchmark_selfeval_ctl.c CTL_SOURCE_FILES -o benchmark.exe -lpsapi

运行：
test_kem.exe
benchmark.exe 10000

说明：
- benchmark.exe 后面的 10000 是测试次数，可修改，但不建议低于 100。
- 在 MinGW/Windows 下，资源消耗测试使用 GetProcessMemoryInfo，因此 benchmark.exe 编译时需要加 -lpsapi。
- CTL 是密钥封装算法，所以签名长度、密钥交换消息长度和密钥交换轮数在结果中标记为 NA。
