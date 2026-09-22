# fast-sort

C 语言高性能排序算法集合：内省排序（Introsort）与基数排序（Radix Sort）。

## 功能

- `introsort_i64(int64_t *a, size_t n)`：通用整型排序
  - 快速排序为主体（三数取中，避免有序数据退化）
  - 子数组长度 ≤ 32 时切换插入排序
  - 递归深度超过 2·log₂n 时退化为堆排序，保证最坏情况 O(n log n)
- `radix_sort_u64(uint64_t *a, size_t n)`：无符号整数专用，O(n)
  - 8 位一趟共 8 趟 LSD 基数排序；有符号整数可通过翻转符号位使用

## 编译与运行

```bash
gcc -O2 -o fast_sort fast_sort.c
./fast_sort
```

macOS 若遇到 SDK 链接问题，可指定旧版 SDK：

```bash
clang -O2 -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk -o fast_sort fast_sort.c
```

## 适用场景

| 场景 | 选谁 |
|---|---|
| 通用整数、数据分布未知 | 内省排序（稳妥的默认选择） |
| 海量纯整数（ID、哈希、时间戳），内存充足 | 基数排序（最快，O(n)） |
| 几万以内的小数组 | 两者差别不大，内省排序更简单 |

- **内省排序**：通用型选手。三数取中使其在有序/近有序随机、含大量重复元素的数据上都不退化；递归过深自动转堆排序，任意输入最坏仍是 O(n log n)。适用于任意 `int64_t`（含负数），稍加改造可用于 double、字符串、结构体。
- **基数排序**：整数专用爆发型。数据量越大优势越明显（百万级比内省排序快约 5 倍）；但小数组反而慢、需额外 n×8 字节内存、只支持无符号整数（有符号数翻转符号位即可）。

## 实测性能（500 万个随机 int64，Apple Silicon，-O2）

| 方法 | 耗时 |
|---|---|
| 内省排序 | 0.22 s |
| 标准库 qsort（对照） | 0.38 s |
| 基数排序（uint64） | 0.04 s |

## 许可

[Apache License 2.0](LICENSE)
