// Khung test tối giản cho logic chạy trên máy tính (không cần ESP32). Mỗi chương trình test là một file .c.
#pragma once
#include <stdio.h>

static int t_run, t_fail;

#define CHECK(cond)                                                                  \
    do {                                                                             \
        t_run++;                                                                     \
        if (!(cond)) {                                                               \
            t_fail++;                                                                \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                 \
        }                                                                            \
    } while (0)

#define CHECK_EQ(a, b)                                                               \
    do {                                                                             \
        long _a = (long) (a), _b = (long) (b);                                       \
        t_run++;                                                                     \
        if (_a != _b) {                                                              \
            t_fail++;                                                                \
            printf("  FAIL %s:%d: %s = %ld, mong doi %ld\n", __FILE__, __LINE__, #a, _a, _b); \
        }                                                                            \
    } while (0)

#define TEST(name) static void name(void)
#define RUN(name) (printf("- %s\n", #name), name())
#define REPORT() (printf("%d/%d kiem tra dat\n", t_run - t_fail, t_run), t_fail ? 1 : 0)
