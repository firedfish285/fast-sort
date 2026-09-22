/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * fast_sort.c — 高性能排序示例
 *
 * 1) introsort_i64: 内省排序（Introsort），对 int64_t 通用数组排序
 *    - 快速排序为主体（三数取中 + 三向划分处理大量重复元素）
 *    - 子数组长度 <= 32 时切换插入排序（小数组更快，且消除递归尾部）
 *    - 递归深度超过 2*log2(n) 时退化为堆排序，保证最坏情况 O(n log n)
 *
 * 2) radix_sort_u64: LSD 基数排序，仅适用于无符号整数（uint64_t）
 *    - 8 位一个 pass，共 8 次，时间复杂度为 O(n)，是最快的整数排序
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

/* ---------------- 内省排序（int64_t 版本） ---------------- */

static inline void swap_i64(int64_t *a, int64_t *b) {
    int64_t t = *a; *a = *b; *b = t;
}

/* 插入排序：适合 n 较小的情形（经验阈值 16~32）或基本有序的数组 */
static void insertion_sort(int64_t *a, size_t lo, size_t hi) { /* 排序 [lo, hi] */
    for (size_t i = lo + 1; i <= hi; i++) {
        int64_t key = a[i];
        size_t j = i;
        while (j > lo && a[j - 1] > key) {
            a[j] = a[j - 1];
            j--;
        }
        a[j] = key;
    }
}

/* 堆排序（sift-down 实现），用于递归过深时兜底，保证最坏 O(n log n) */
static void sift_down(int64_t *a, size_t root, size_t n) {
    for (;;) {
        size_t child = 2 * root + 1;         /* 左孩子 */
        if (child >= n) break;
        if (child + 1 < n && a[child + 1] > a[child]) child++; /* 取较大孩子 */
        if (a[root] >= a[child]) break;
        swap_i64(&a[root], &a[child]);
        root = child;
    }
}

static void heap_sort(int64_t *a, size_t n) {
    if (n < 2) return;
    for (size_t i = n / 2; i-- > 0; ) sift_down(a, i, n); /* 建堆 */
    for (size_t end = n - 1; end > 0; end--) {
        swap_i64(&a[0], &a[end]);
        sift_down(a, 0, end);
    }
}

/* 三数取中，并把中位数放到 a[hi-1] 作为枢轴（哨兵技巧） */
static inline int64_t median3(int64_t *a, size_t lo, size_t mid, size_t hi) {
    if (a[lo] > a[mid]) swap_i64(&a[lo], &a[mid]);
    if (a[lo] > a[hi])  swap_i64(&a[lo], &a[hi]);
    if (a[mid] > a[hi]) swap_i64(&a[mid], &a[hi]);
    swap_i64(&a[mid], &a[hi - 1]);  /* 枢轴放到 hi-1 */
    return a[hi - 1];
}

#define INSERTION_THRESHOLD 32

static void introsort_rec(int64_t *a, size_t lo, size_t hi, int depth_limit) {
    while (hi - lo + 1 > INSERTION_THRESHOLD) {
        if (depth_limit == 0) {              /* 递归过深 -> 堆排序兜底 */
            heap_sort(a + lo, hi - lo + 1);
            return;
        }
        depth_limit--;

        size_t mid = lo + (hi - lo) / 2;
        int64_t pivot = median3(a, lo, mid, hi);

        /* 双向划分：[lo, hi-2] 参与划分，枢轴在 hi-1 */
        size_t p = lo, q = hi - 2;
        for (;;) {
            while (a[p] < pivot) p++;
            while (a[q] > pivot) q--;
            if (p >= q) break;
            swap_i64(&a[p], &a[q]);
            p++; q--;
        }
        /* 把枢轴（在 hi-1）放到正确位置 */
        swap_i64(&a[p], &a[hi - 1]);

        /* 递归处理较小的一半，较大的一半用循环消除尾递归 */
        if (p - lo < hi - p) {
            introsort_rec(a, lo, p > 0 ? p - 1 : 0, depth_limit);
            lo = p + 1;
        } else {
            introsort_rec(a, p + 1, hi, depth_limit);
            hi = p > 0 ? p - 1 : 0;
        }
    }
    insertion_sort(a, lo, hi);
}

void introsort_i64(int64_t *a, size_t n) {
    if (n < 2) return;
    int depth_limit = 0;
    for (size_t m = n; m > 1; m >>= 1) depth_limit++; /* floor(log2 n) */
    depth_limit *= 2;
    introsort_rec(a, 0, n - 1, depth_limit);
}

/* ---------------- 基数排序（仅无符号整数，O(n)） ---------------- */

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

/* ---------------- 测试与基准 ---------------- */

static int cmp_i64(const void *x, const void *y) {
    int64_t a = *(const int64_t *)x, b = *(const int64_t *)y;
    return (a > b) - (a < b);
}

static bool is_sorted_i64(const int64_t *a, size_t n) {
    for (size_t i = 1; i < n; i++) if (a[i - 1] > a[i]) return false;
    return true;
}

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(void) {
    const size_t N = 5000000;
    int64_t *a = malloc(N * sizeof(int64_t));
    int64_t *b = malloc(N * sizeof(int64_t));
    if (!a || !b) { fprintf(stderr, "malloc failed\n"); return 1; }

    srand(42);
    for (size_t i = 0; i < N; i++) a[i] = ((int64_t)rand() << 32) ^ rand();
    memcpy(b, a, N * sizeof(int64_t));

    /* 正确性抽检 */
    int64_t small[] = {5, -3, 5, 0, 9, 1, 1, -100, 7, 2, 2, 2};
    introsort_i64(small, sizeof(small) / sizeof(small[0]));
    if (!is_sorted_i64(small, sizeof(small) / sizeof(small[0]))) {
        printf("introsort WRONG on small case\n"); return 1;
    }

    double t0 = now_sec();
    introsort_i64(a, N);
    double t1 = now_sec();
    printf("introsort:  %.3f s  sorted=%d\n", t1 - t0, is_sorted_i64(a, N));

    t0 = now_sec();
    qsort(b, N, sizeof(int64_t), cmp_i64);   /* 标准库对照 */
    t1 = now_sec();
    printf("qsort:      %.3f s  sorted=%d\n", t1 - t0, is_sorted_i64(b, N));

    /* 有符号整数不能直接基数排序，演示用无符号版本 */
    uint64_t *u = malloc(N * sizeof(uint64_t));
    for (size_t i = 0; i < N; i++) u[i] = (uint64_t)a[i] ^ 0x8000000000000000ULL; /* 还原乱序 */
    t0 = now_sec();
    radix_sort_u64(u, N);
    t1 = now_sec();
    printf("radix(u64): %.3f s  sorted=%d\n", t1 - t0, is_sorted_i64((int64_t *)u, N));

    free(a); free(b); free(u);
    return 0;
}
