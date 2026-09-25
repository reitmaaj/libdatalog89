#ifndef DL89_TEST_H
#define DL89_TEST_H

#include <stdio.h>

extern int dl89_test_failures;

#define T_ASSERT(expr)                                                         \
    do                                                                         \
    {                                                                          \
        if (!(expr))                                                           \
        {                                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);    \
            dl89_test_failures = dl89_test_failures + 1;                       \
        }                                                                      \
    } while (0)

#define T_STATUS(expr, expected)                                               \
    do                                                                         \
    {                                                                          \
        int t_status_ = (expr);                                                \
        if (t_status_ != (expected))                                           \
        {                                                                      \
            fprintf(stderr, "FAIL %s:%d: %s => %d (expected %d)\n", __FILE__,  \
                    __LINE__, #expr, t_status_, (expected));                   \
            dl89_test_failures = dl89_test_failures + 1;                       \
        }                                                                      \
    } while (0)

#define T_EQ_SIZE(a, b)                                                        \
    do                                                                         \
    {                                                                          \
        size_t t_a_ = (size_t)(a);                                             \
        size_t t_b_ = (size_t)(b);                                             \
        if (t_a_ != t_b_)                                                      \
        {                                                                      \
            fprintf(stderr, "FAIL %s:%d: %s==%s (%lu != %lu)\n", __FILE__,     \
                    __LINE__, #a, #b, (unsigned long)t_a_,                     \
                    (unsigned long)t_b_);                                      \
            dl89_test_failures = dl89_test_failures + 1;                       \
        }                                                                      \
    } while (0)

#endif
