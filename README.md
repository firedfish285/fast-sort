# fast-sort

C 语言高性能排序算法集合：内省排序（Introsort）与基数排序（Radix Sort），支持整型、浮点与字符串。

## 功能

### 内省排序（Introsort，宏模板生成，最坏 O(n log n)）

同一套实现经宏模板（X-macro）展开为三种元素类型：

- `introsort_i64(int64_t *a, size_t n)`：通用整型排序（含负数）
- `introsort_dbl(double *a, size_t n)`：浮点排序（不支持 NaN）
- `introsort_str(char **a, size_t n)`：字符串指针数组排序（字典序，不移动串本身）

实现要点：

- 快速排序为主体（三数取中，避免有序数据退化）
- 子数组长度 ≤ 32 时切换插入排序
- 递归深度超过 2·log₂n 时退化为堆排序，保证最坏情况 O(n log n)

### 基数排序（Radix Sort，O(n)）

- `radix_sort_u64(uint64_t *a, size_t n)`：LSD，8 位一趟共 8 趟；有符号整数可翻转符号位使用
- `radix_sort_dbl(double *a, size_t n)`：把 IEEE 754 位模式映射为可排序整数（负数按位取反、正数置符号位）后复用 u64 版本；同样不支持 NaN
- `radix_sort_str(char **a, size_t n)`：MSD 高位优先，逐字符计数排序（稳定），短串先于长前缀串；子数组 ≤ 16 时切换后缀插入排序，需额外 n×8 字节辅助数组

## 编译与运行

```bash
make          # 或 gcc -O2 -Wall -Wextra -o fast_sort fast_sort.c
make test     # 编译并运行正确性测试 + 基准测试
```

macOS 若遇到 SDK 链接问题（如 `tapi error: malformed file`），可指定旧版 SDK：

```bash
make SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk
```

## 适用场景

| 场景 | 选谁 |
|---|---|
| 通用整数/浮点/字符串、数据分布未知 | 内省排序（稳妥的默认选择） |
| 海量纯整数（ID、哈希、时间戳），内存充足 | 基数排序（最快，O(n)） |
| 海量浮点 | `radix_sort_dbl`（映射后同样是 O(n)） |
| 字符串且公共前缀多 | `radix_sort_str`（MSD 天然跳过公共前缀） |
| 几万以内的小数组 | 两者差别不大，内省排序更简单 |

- **内省排序**：通用型选手。三数取中使其在有序/近有序随机、含大量重复元素的数据上都不退化；递归过深自动转堆排序，任意输入最坏仍是 O(n log n)。经宏模板支持 `int64_t`、`double`、字符串，扩展新类型只需一行（如 `float`、结构体指针）。
- **基数排序**：整数专用爆发型。数据量越大优势越明显（百万级比内省排序快约 4~6 倍）；但小数组反而慢、需额外内存。double 版本借助位技巧复用同一实现；字符串版本为 MSD 实现，避免比较排序反复扫描公共前缀。

## 实测性能（Apple Silicon，-O2）

int64（500 万个随机数）：

| 方法 | 耗时 |
|---|---|
| 内省排序 | 0.206 s |
| 标准库 qsort（对照） | 0.327 s |
| 基数排序（uint64） | 0.058 s |

double（500 万个随机数）：

| 方法 | 耗时 |
|---|---|
| 内省排序 | 0.283 s |
| 标准库 qsort（对照） | 0.421 s |
| 基数排序 | 0.046 s |

string（100 万个随机串，长度 1~12）：

| 方法 | 耗时 |
|---|---|
| 内省排序 | 0.127 s |
| 标准库 qsort（对照） | 0.140 s |
| 基数排序（MSD） | 0.021 s |

## 许可

[Apache License 2.0](LICENSE)
