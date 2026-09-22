/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * fast_sort.c — 高性能排序算法集合
 *
 * 内省排序（Introsort，宏模板生成三种元素类型，同一套实现）
 *   - introsort_i64: int64_t 通用数组
 *   - introsort_dbl: double 浮点数组（不支持 NaN，传入 NaN 顺序不保证）
 *   - introsort_str: 字符串指针数组（按 strcmp 字典序，短串先于长前缀串）
 *   实现要点：快速排序为主体（三数取中 + 双向划分）、子数组 ≤ 32 转插入排序、
 *   递归深度超过 2*log2(n) 退化为堆排序，任意输入最坏 O(n log n)
 *
 * 基数排序（Radix Sort，O(n)）
 *   - radix_sort_u64: LSD，8 位 × 8 趟，仅适用 uint64_t
 *   - radix_sort_dbl: 借助位技巧（负数按位取反、正数置符号位）复用 u64 版本
 *   - radix_sort_str: MSD，逐字符计数排序（稳定），小数组转后缀插入排序
 *
 * 编译: gcc -O2 -o fast_sort fast_sort.c
 * 运行: ./fast_sort
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* ---------------- 内省排序宏模板 ----------------
 * 一套代码经宏展开生成不同元素类型的实现：
 *   NAME —— 生成函数后缀（introsort_NAME 及内部辅助函数）
 *   TYPE —— 元素类型
 *   LESS —— 二元“小于”判定宏，形如 LESS(x, y)
 */

#define INSERTION_THRESHOLD 32

#define DEFINE_INTROSORT(NAME, TYPE, LESS)                                     \
                                                                               \
static inline void swap_##NAME(TYPE *a, TYPE *b) {                             \
    TYPE t_ = *a; *a = *b; *b = t_;                                            \
}                                                                              \
                                                                               \
/* 插入排序：适合 n 较小的情形（经验阈值 16~32）或基本有序的数组 */            \
static void insertion_sort_##NAME(TYPE *a, size_t lo, size_t hi) {             \
    for (size_t i = lo + 1; i <= hi; i++) {                                    \
        TYPE key_ = a[i];                                                      \
        size_t j = i;                                                          \
        while (j > lo && LESS(key_, a[j - 1])) {                               \
            a[j] = a[j - 1];                                                   \
            j--;                                                               \
        }                                                                      \
        a[j] = key_;                                                           \
    }                                                                          \
}                                                                              \
                                                                               \
/* 堆排序（sift-down 实现），用于递归过深时兜底，保证最坏 O(n log n) */        \
static void sift_down_##NAME(TYPE *a, size_t root, size_t n) {                 \
    for (;;) {                                                                 \
        size_t child = 2 * root + 1;         /* 左孩子 */                      \
        if (child >= n) break;                                                 \
        if (child + 1 < n && LESS(a[child], a[child + 1])) child++;            \
        if (!LESS(a[child], a[root])) break;                                   \
        swap_##NAME(&a[root], &a[child]);                                      \
        root = child;                                                          \
    }                                                                          \
}                                                                              \
                                                                               \
static void heap_sort_##NAME(TYPE *a, size_t n) {                              \
    if (n < 2) return;                                                         \
    for (size_t i = n / 2; i-- > 0; ) sift_down_##NAME(a, i, n); /* 建堆 */    \
    for (size_t end = n - 1; end > 0; end--) {                                 \
        swap_##NAME(&a[0], &a[end]);                                           \
        sift_down_##NAME(a, 0, end);                                           \
    }                                                                          \
}                                                                              \
                                                                               \
/* 三数取中，并把中位数放到 a[hi-1] 作为枢轴（哨兵技巧） */                    \
static inline TYPE median3_##NAME(TYPE *a, size_t lo, size_t mid, size_t hi) { \
    if (LESS(a[mid], a[lo])) swap_##NAME(&a[lo], &a[mid]);                     \
    if (LESS(a[hi], a[lo]))  swap_##NAME(&a[lo], &a[hi]);                      \
    if (LESS(a[hi], a[mid])) swap_##NAME(&a[mid], &a[hi]);                     \
    swap_##NAME(&a[mid], &a[hi - 1]);  /* 枢轴放到 hi-1 */                     \
    return a[hi - 1];                                                          \
}                                                                              \
                                                                               \
static void introsort_rec_##NAME(TYPE *a, size_t lo, size_t hi,                \
                                 int depth_limit) {                            \
    while (hi - lo + 1 > INSERTION_THRESHOLD) {                                \
        if (depth_limit == 0) {              /* 递归过深 -> 堆排序兜底 */      \
            heap_sort_##NAME(a + lo, hi - lo + 1);                             \
            return;                                                            \
        }                                                                      \
        depth_limit--;                                                         \
                                                                               \
        size_t mid = lo + (hi - lo) / 2;                                       \
        TYPE pivot = median3_##NAME(a, lo, mid, hi);                           \
                                                                               \
        /* 双向划分：[lo, hi-2] 参与划分，枢轴在 hi-1 */                        \
        size_t p = lo, q = hi - 2;                                             \
        for (;;) {                                                             \
            while (LESS(a[p], pivot)) p++;                                     \
            while (LESS(pivot, a[q])) q--;                                     \
            if (p >= q) break;                                                 \
            swap_##NAME(&a[p], &a[q]);                                         \
            p++; q--;                                                          \
        }                                                                      \
        /* 把枢轴（在 hi-1）放到正确位置 */                                     \
        swap_##NAME(&a[p], &a[hi - 1]);                                        \
                                                                               \
        /* 递归处理较小的一半，较大的一半用循环消除尾递归 */                    \
        if (p - lo < hi - p) {                                                 \
            introsort_rec_##NAME(a, lo, p > 0 ? p - 1 : 0, depth_limit);       \
            lo = p + 1;                                                        \
        } else {                                                               \
            introsort_rec_##NAME(a, p + 1, hi, depth_limit);                   \
            hi = p > 0 ? p - 1 : 0;                                            \
        }                                                                      \
    }                                                                          \
    insertion_sort_##NAME(a, lo, hi);                                          \
}                                                                              \
                                                                               \
void introsort_##NAME(TYPE *a, size_t n) {                                     \
    if (n < 2) return;                                                         \
    int depth_limit = 0;                                                       \
    for (size_t m = n; m > 1; m >>= 1) depth_limit++; /* floor(log2 n) */      \
    depth_limit *= 2;                                                          \
    introsort_rec_##NAME(a, 0, n - 1, depth_limit);                            \
}

/* 数值与字符串的“小于”判定 */
#define LESS_NUM(x, y) ((x) < (y))
#define LESS_STR(x, y) (strcmp((x), (y)) < 0)

DEFINE_INTROSORT(i64, int64_t, LESS_NUM)
DEFINE_INTROSORT(dbl, double, LESS_NUM)
DEFINE_INTROSORT(str, char *, LESS_STR)

/* ---------------- 基数排序（无符号整数，LSD，O(n)） ---------------- */

void radix_sort_u64(uint64_t *a, size_t n) {
    if (n < 2) return;
    uint64_t *buf = malloc(n * sizeof(uint64_t));
    if (!buf) { introsort_i64((int64_t *)a, n); return; } /* 内存不够则回退 */

    uint64_t *src = a, *dst = buf;
    for (int shift = 0; shift < 64; shift += 8) {
        size_t count[256] = {0}, offset[256];
        for (size_t i = 0; i < n; i++) count[(src[i] >> shift) & 0xff]++;
        offset[0] = 0;
        for (int b = 1; b < 256; b++) offset[b] = offset[b - 1] + count[b - 1];
        for (size_t i = 0; i < n; i++) {
            size_t b = (src[i] >> shift) & 0xff;
            dst[offset[b]++] = src[i];
        }
        uint64_t *t = src; src = dst; dst = t;   /* 交换缓冲 */
    }
    if (src != a) memcpy(a, src, n * sizeof(uint64_t)); /* 奇数次 pass 后拷回 */
    free(buf);
}

/* ---------------- 基数排序（double） ----------------
 * IEEE 754 位模式与大小序的映射：负数按位取反（越负越小），
 * 正数（含 +0）置符号位，使映射后的 uint64 大小序与原 double 一致。
 */

static inline uint64_t dbl_to_key(double x) {
    uint64_t u;
    memcpy(&u, &x, sizeof u);
    return (u & 0x8000000000000000ULL) ? ~u : (u | 0x8000000000000000ULL);
}

static inline double key_to_dbl(uint64_t u) {
    uint64_t b = (u & 0x8000000000000000ULL) ? (u ^ 0x8000000000000000ULL) : ~u;
    double x;
    memcpy(&x, &b, sizeof x);
    return x;
}

void radix_sort_dbl(double *a, size_t n) {
    if (n < 2) return;
    uint64_t *u = (uint64_t *)a;  /* 逐元素 memcpy 搬运位模式，无严格别名问题 */
    for (size_t i = 0; i < n; i++) {
        uint64_t k = dbl_to_key(a[i]);
        memcpy(&u[i], &k, sizeof k);
    }
    radix_sort_u64(u, n);
    for (size_t i = 0; i < n; i++) {
        uint64_t k;
        memcpy(&k, &u[i], sizeof k);
        a[i] = key_to_dbl(k);
    }
}

/* ---------------- 基数排序（字符串，MSD，高位优先） ----------------
 * 逐字符做计数排序（按 unsigned char，稳定），短串天然排在长前缀串之前；
 * 子数组长度 ≤ MSD_CUTOFF 时切换为从第 d 个字符开始的后缀插入排序。
 * 需要额外的 n×sizeof(char*) 辅助数组。
 */

#define RADIX_R    256
#define MSD_CUTOFF 16

/* 取第 d 个字符（无符号）；串已结束返回 -1 */
static inline int char_at(const char *s, size_t d) {
    return s[d] == '\0' ? -1 : (unsigned char)s[d];
}

/* 对 a[lo..hi] 从第 d 个字符起的后缀做插入排序（稳定） */
static void insertion_sort_suffix(char **a, size_t lo, size_t hi, size_t d) {
    for (size_t i = lo + 1; i <= hi; i++) {
        char *key = a[i];
        size_t j = i;
        while (j > lo && strcmp(a[j - 1] + d, key + d) > 0) {
            a[j] = a[j - 1];
            j--;
        }
        a[j] = key;
    }
}

static void msd_rec(char **a, char **aux, size_t lo, size_t hi, size_t d) {
    if (hi <= lo + MSD_CUTOFF) {          /* 小数组：后缀插入排序更快 */
        insertion_sort_suffix(a, lo, hi, d);
        return;
    }

    /* count[0] 放“串已结束”(-1)的桶，count[c+2] 放字符 c 的桶 */
    size_t count[RADIX_R + 2] = {0};
    for (size_t i = lo; i <= hi; i++)
        count[(size_t)(char_at(a[i], d) + 2)]++;
    for (int r = 0; r < RADIX_R + 1; r++)
        count[r + 1] += count[r];         /* 前缀和 -> 桶的起始偏移 */

    for (size_t i = lo; i <= hi; i++) {
        int c = char_at(a[i], d);
        aux[lo + count[c + 1]++] = a[i];  /* 稳定分发 */
    }
    for (size_t i = lo; i <= hi; i++)
        a[i] = aux[i];

    /* 递归各字符桶；“已结束”桶本身有序，跳过（size_t 下溢保护：跳过空桶） */
    for (int r = 0; r < RADIX_R; r++)
        if (count[r] < count[r + 1])
            msd_rec(a, aux, lo + count[r], lo + count[r + 1] - 1, d + 1);
}

void radix_sort_str(char **a, size_t n) {
    if (n < 2) return;
    char **aux = malloc(n * sizeof(char *));
    if (!aux) { introsort_str(a, n); return; } /* 内存不够则回退 */
    msd_rec(a, aux, 0, n - 1, 0);
    free(aux);
}

/* ---------------- 测试 ---------------- */

static int g_failures = 0;

#define EXPECT(cond, msg) do {                                                 \
    if (!(cond)) {                                                             \
        printf("    [FAIL] %s (line %d)\n", msg, __LINE__);                    \
        g_failures++;                                                          \
    }                                                                          \
} while (0)

static int cmp_i64(const void *x, const void *y) {
    int64_t a = *(const int64_t *)x, b = *(const int64_t *)y;
    return (a > b) - (a < b);
}

static int cmp_u64(const void *x, const void *y) {
    uint64_t a = *(const uint64_t *)x, b = *(const uint64_t *)y;
    return (a > b) - (a < b);
}

static int cmp_dbl(const void *x, const void *y) {
    double a = *(const double *)x, b = *(const double *)y;
    return (a > b) - (a < b);
}

static int cmp_str(const void *x, const void *y) {
    return strcmp(*(const char *const *)x, *(const char *const *)y);
}

static bool is_sorted_i64(const int64_t *a, size_t n) {
    for (size_t i = 1; i < n; i++) if (a[i - 1] > a[i]) return false;
    return true;
}

static bool is_sorted_dbl(const double *a, size_t n) {
    for (size_t i = 1; i < n; i++) if (a[i - 1] > a[i]) return false;
    return true;
}

static bool is_sorted_str(char *const *a, size_t n) {
    for (size_t i = 1; i < n; i++) if (strcmp(a[i - 1], a[i]) > 0) return false;
    return true;
}

/* 生成随机 int64，1/4 概率压到千以内以制造重复 */
static int64_t rand_i64(void) {
    int64_t v = ((int64_t)rand() << 32) ^ rand();
    return rand() % 4 == 0 ? v % 1000 : v;
}

/* 生成随机 double：整数/100，有正有负且含大量重复，不产生 NaN/±0 歧义 */
static double rand_dbl(void) {
    return (double)((int64_t)rand() % 2000000 - 1000000) / 100.0;
}

/* 生成随机字符串：小字母表（大量公共前缀）+ 随机长度（含空串） */
static char *rand_string(void) {
    size_t len = (size_t)(rand() % 12);
    char *s = malloc(len + 1);
    for (size_t i = 0; i < len; i++) s[i] = (char)('a' + rand() % 3);
    s[len] = '\0';
    return s;
}

static void report(const char *name, int before) {
    printf("  %-18s %s\n", name, g_failures == before ? "OK" : "FAIL");
}

static void test_introsort_i64(void) {
    int before = g_failures;
    int64_t small[] = {5, -3, 5, 0, 9, 1, 1, -100, 7, 2, 2, 2};
    introsort_i64(small, 12);
    EXPECT(is_sorted_i64(small, 12), "introsort_i64 small case");

    introsort_i64(NULL, 0);
    int64_t one = 42;
    introsort_i64(&one, 1);
    EXPECT(one == 42, "introsort_i64 n=0/1");

    /* 全相同 + 正序 + 逆序 */
    static int64_t eq[1000], asc[10000], desc[10000];
    for (size_t i = 0; i < 1000; i++) eq[i] = 7;
    for (size_t i = 0; i < 10000; i++) { asc[i] = (int64_t)i; desc[i] = (int64_t)(10000 - i); }
    introsort_i64(eq, 1000);  introsort_i64(asc, 10000); introsort_i64(desc, 10000);
    EXPECT(is_sorted_i64(eq, 1000) && is_sorted_i64(asc, 10000)
           && is_sorted_i64(desc, 10000), "introsort_i64 equal/asc/desc");

    /* 随机模糊测试：与 qsort 结果逐一比对 */
    const size_t sizes[] = {0, 1, 2, 3, 17, 32, 33, 100, 1000, 5000};
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        size_t n = sizes[s];
        int64_t *a = malloc((n ? n : 1) * sizeof(int64_t));
        int64_t *ref = malloc((n ? n : 1) * sizeof(int64_t));
        for (int t = 0; t < 10; t++) {
            for (size_t i = 0; i < n; i++) ref[i] = a[i] = rand_i64();
            introsort_i64(a, n);
            qsort(ref, n, sizeof(int64_t), cmp_i64);
            EXPECT(memcmp(a, ref, n * sizeof(int64_t)) == 0, "introsort_i64 fuzz");
        }
        free(a); free(ref);
    }
    report("introsort_i64", before);
}

static void test_introsort_dbl(void) {
    int before = g_failures;
    /* 特殊值：极值、次正规数（denormal）、±0、重复 */
    double special[] = {1e308, -1e308, 0.0, -0.0, 1e-308, -1e-308, 5e-324, -5e-324,
                        3.141592653589793, -2.718281828459045, 1.0, -1.0, 0.5,
                        42.0, -42.0, 42.0, 1.7976931348623157e308, 2.2250738585072014e-308};
    size_t ns = sizeof special / sizeof special[0];
    double *ref = malloc(ns * sizeof(double));
    memcpy(ref, special, sizeof special);
    introsort_dbl(special, ns);
    qsort(ref, ns, sizeof(double), cmp_dbl);
    bool same = is_sorted_dbl(special, ns);
    for (size_t i = 0; i < ns; i++) if (special[i] != ref[i]) same = false;
    EXPECT(same, "introsort_dbl special values");
    free(ref);

    introsort_dbl(NULL, 0);
    double one = -0.5;
    introsort_dbl(&one, 1);
    EXPECT(one == -0.5, "introsort_dbl n=0/1");

    const size_t sizes[] = {0, 1, 2, 3, 17, 32, 33, 100, 1000, 5000};
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        size_t n = sizes[s];
        double *a = malloc((n ? n : 1) * sizeof(double));
        double *r = malloc((n ? n : 1) * sizeof(double));
        for (int t = 0; t < 10; t++) {
            for (size_t i = 0; i < n; i++) r[i] = a[i] = rand_dbl();
            introsort_dbl(a, n);
            qsort(r, n, sizeof(double), cmp_dbl);
            bool ok = is_sorted_dbl(a, n);
            for (size_t i = 0; i < n; i++) if (a[i] != r[i]) ok = false;
            EXPECT(ok, "introsort_dbl fuzz");
        }
        free(a); free(r);
    }
    report("introsort_dbl", before);
}

static void test_introsort_str(void) {
    int before = g_failures;
    /* 固定用例：空串、公共前缀、重复、高位字节（UTF-8 片段） */
    char *fixed[] = {"z", "abc", "", "abd", "ab", "a", "abc", "abce", "abcd", "b",
                     "\303\251clair", "apple"};
    size_t nf = sizeof fixed / sizeof fixed[0];
    char *expected[] = {"", "a", "ab", "abc", "abc", "abcd", "abce", "abd",
                        "apple", "b", "z", "\303\251clair"};
    char **work = malloc(nf * sizeof(char *));
    memcpy(work, fixed, sizeof fixed);
    introsort_str(work, nf);
    bool ok = is_sorted_str(work, nf);
    for (size_t i = 0; i < nf; i++)
        if (strcmp(work[i], expected[i]) != 0) ok = false;
    EXPECT(ok, "introsort_str fixed case");
    free(work);

    introsort_str(NULL, 0);
    char *single[] = {"solo"};
    introsort_str(single, 1);
    EXPECT(strcmp(single[0], "solo") == 0, "introsort_str n=0/1");

    const size_t sizes[] = {1, 2, 3, 17, 33, 100, 1000, 5000};
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        size_t n = sizes[s];
        char **a = malloc(n * sizeof(char *));
        char **r = malloc(n * sizeof(char *));
        for (int t = 0; t < 10; t++) {
            for (size_t i = 0; i < n; i++) r[i] = a[i] = rand_string();
            introsort_str(a, n);
            qsort(r, n, sizeof(char *), cmp_str);
            bool good = is_sorted_str(a, n);
            for (size_t i = 0; i < n; i++)
                if (strcmp(a[i], r[i]) != 0) good = false;
            EXPECT(good, "introsort_str fuzz");
            for (size_t i = 0; i < n; i++) free(a[i]);
        }
        free(a); free(r);
    }
    report("introsort_str", before);
}

static void test_radix_u64(void) {
    int before = g_failures;
    radix_sort_u64(NULL, 0);
    uint64_t one = 99;
    radix_sort_u64(&one, 1);
    EXPECT(one == 99, "radix_sort_u64 n=0/1");

    const size_t sizes[] = {2, 33, 100, 1000, 5000};
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        size_t n = sizes[s];
        uint64_t *a = malloc(n * sizeof(uint64_t));
        uint64_t *r = malloc(n * sizeof(uint64_t));
        for (int t = 0; t < 10; t++) {
            for (size_t i = 0; i < n; i++) {
                r[i] = ((uint64_t)rand() << 32) ^ (uint64_t)rand();
                if (rand() % 4 == 0) r[i] %= 1000;  /* 制造重复 */
                a[i] = r[i];
            }
            radix_sort_u64(a, n);
            qsort(r, n, sizeof(uint64_t), cmp_u64);
            EXPECT(memcmp(a, r, n * sizeof(uint64_t)) == 0, "radix_sort_u64 fuzz");
        }
        free(a); free(r);
    }
    report("radix_sort_u64", before);
}

static void test_radix_dbl(void) {
    int before = g_failures;
    radix_sort_dbl(NULL, 0);
    double one = -7.25;
    radix_sort_dbl(&one, 1);
    EXPECT(one == -7.25, "radix_sort_dbl n=0/1");

    const size_t sizes[] = {2, 33, 100, 1000, 5000};
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        size_t n = sizes[s];
        double *a = malloc(n * sizeof(double));
        double *r = malloc(n * sizeof(double));
        for (int t = 0; t < 10; t++) {
            for (size_t i = 0; i < n; i++) r[i] = a[i] = rand_dbl();
            radix_sort_dbl(a, n);
            qsort(r, n, sizeof(double), cmp_dbl);
            bool ok = is_sorted_dbl(a, n);
            for (size_t i = 0; i < n; i++) if (a[i] != r[i]) ok = false;
            EXPECT(ok, "radix_sort_dbl fuzz");
        }
        free(a); free(r);
    }
    report("radix_sort_dbl", before);
}

static void test_radix_str(void) {
    int before = g_failures;
    radix_sort_str(NULL, 0);
    char *single[] = {"solo"};
    radix_sort_str(single, 1);
    EXPECT(strcmp(single[0], "solo") == 0, "radix_sort_str n=0/1");

    const size_t sizes[] = {2, 17, 33, 100, 1000, 5000};
    for (size_t s = 0; s < sizeof sizes / sizeof sizes[0]; s++) {
        size_t n = sizes[s];
        char **a = malloc(n * sizeof(char *));
        char **r = malloc(n * sizeof(char *));
        for (int t = 0; t < 10; t++) {
            for (size_t i = 0; i < n; i++) r[i] = a[i] = rand_string();
            radix_sort_str(a, n);
            qsort(r, n, sizeof(char *), cmp_str);
            bool ok = is_sorted_str(a, n);
            for (size_t i = 0; i < n; i++)
                if (strcmp(a[i], r[i]) != 0) ok = false;
            EXPECT(ok, "radix_sort_str fuzz");
            for (size_t i = 0; i < n; i++) free(a[i]);
        }
        free(a); free(r);
    }
    report("radix_sort_str", before);
}

static void run_all_tests(void) {
    test_introsort_i64();
    test_introsort_dbl();
    test_introsort_str();
    test_radix_u64();
    test_radix_dbl();
    test_radix_str();
}

/* ---------------- 基准 ---------------- */

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static void bench_i64(void) {
    const size_t N = 5000000;
    printf("— int64_t（%zu 个随机数）—\n", N);
    int64_t *a = malloc(N * sizeof(int64_t));
    int64_t *b = malloc(N * sizeof(int64_t));
    if (!a || !b) { fprintf(stderr, "malloc failed\n"); exit(1); }
    for (size_t i = 0; i < N; i++) b[i] = a[i] = rand_i64();

    double t0 = now_sec();
    introsort_i64(a, N);
    double t1 = now_sec();
    printf("introsort:  %.3f s  sorted=%d\n", t1 - t0, is_sorted_i64(a, N));

    t0 = now_sec();
    qsort(b, N, sizeof(int64_t), cmp_i64);   /* 标准库对照 */
    t1 = now_sec();
    printf("qsort:      %.3f s  sorted=%d\n", t1 - t0, is_sorted_i64(b, N));

    uint64_t *u = malloc(N * sizeof(uint64_t));
    for (size_t i = 0; i < N; i++) u[i] = (uint64_t)a[i] ^ 0x8000000000000000ULL;
    t0 = now_sec();
    radix_sort_u64(u, N);
    t1 = now_sec();
    printf("radix(u64): %.3f s  sorted=%d\n", t1 - t0, is_sorted_i64((int64_t *)u, N));

    free(a); free(b); free(u);
}

static void bench_dbl(void) {
    const size_t N = 5000000;
    printf("— double（%zu 个随机数）—\n", N);
    double *a = malloc(N * sizeof(double));
    double *b = malloc(N * sizeof(double));
    if (!a || !b) { fprintf(stderr, "malloc failed\n"); exit(1); }
    for (size_t i = 0; i < N; i++) {
        /* 宽动态范围：随机 64 位整数缩小，含大量重复 */
        b[i] = a[i] = (double)(((int64_t)rand() << 32) ^ rand()) / 65536.0;
    }

    double t0 = now_sec();
    introsort_dbl(a, N);
    double t1 = now_sec();
    printf("introsort:  %.3f s  sorted=%d\n", t1 - t0, is_sorted_dbl(a, N));

    t0 = now_sec();
    qsort(b, N, sizeof(double), cmp_dbl);
    t1 = now_sec();
    printf("qsort:      %.3f s  sorted=%d\n", t1 - t0, is_sorted_dbl(b, N));

    for (size_t i = 0; i < N; i++) a[i] = (double)(((int64_t)rand() << 32) ^ rand()) / 65536.0;
    t0 = now_sec();
    radix_sort_dbl(a, N);
    t1 = now_sec();
    printf("radix(dbl): %.3f s  sorted=%d\n", t1 - t0, is_sorted_dbl(a, N));

    free(a); free(b);
}

static void bench_str(void) {
    const size_t N = 1000000;
    printf("— string（%zu 个随机串，长度 1~12）—\n", N);
    char **a = malloc(N * sizeof(char *));
    char **b = malloc(N * sizeof(char *));
    if (!a || !b) { fprintf(stderr, "malloc failed\n"); exit(1); }
    for (size_t i = 0; i < N; i++) {
        size_t len = 1 + (size_t)(rand() % 12);
        char *s = malloc(len + 1);
        for (size_t j = 0; j < len; j++) s[j] = (char)('a' + rand() % 26);
        s[len] = '\0';
        b[i] = a[i] = s;
    }

    double t0 = now_sec();
    introsort_str(a, N);
    double t1 = now_sec();
    printf("introsort:  %.3f s  sorted=%d\n", t1 - t0, is_sorted_str(a, N));

    t0 = now_sec();
    qsort(b, N, sizeof(char *), cmp_str);
    t1 = now_sec();
    printf("qsort:      %.3f s  sorted=%d\n", t1 - t0, is_sorted_str(b, N));

    /* 从有序数组打乱回同一组指针再测基数排序 */
    for (size_t i = 0; i < N; i++) a[i] = b[i];
    t0 = now_sec();
    radix_sort_str(a, N);
    t1 = now_sec();
    printf("radix(str): %.3f s  sorted=%d\n", t1 - t0, is_sorted_str(a, N));

    for (size_t i = 0; i < N; i++) free(a[i]);
    free(a); free(b);
}

int main(void) {
    srand(42);   /* 固定种子，结果可复现 */

    printf("== 正确性测试 ==\n");
    run_all_tests();
    if (g_failures > 0) {
        printf("\n%d 项测试失败\n", g_failures);
        return 1;
    }
    printf("全部通过\n\n== 基准测试 ==\n");
    bench_i64();
    bench_dbl();
    bench_str();
    return 0;
}
