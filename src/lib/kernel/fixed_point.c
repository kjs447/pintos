#include "fixed_point.h"
#include <stdint.h>

#define F (1 << 14)

/* Convert integer to fixed point */
struct fixed_point to_fp(int n) {
    struct fixed_point res;
    res.base = n * F;
    return res;
}

/* Convert fixed point to integer */
int to_int(struct fixed_point fp) {
    return (fp.base + F - 1) / F;
}

/* fp + fp */
struct fixed_point add(struct fixed_point a, struct fixed_point b) {
    struct fixed_point fp;
    fp.base = a.base + b.base;
    return fp;
}

/* fp - fp */
struct fixed_point sub(struct fixed_point a, struct fixed_point b) {
    struct fixed_point fp;
    fp.base = a.base - b.base;
    return fp;
}

/* fp * fp */
struct fixed_point mult(struct fixed_point a, struct fixed_point b) {
    struct fixed_point fp;
    fp.base = (int64_t)a.base * b.base / F;
    return fp;
}

/* fp / fp */
struct fixed_point div(struct fixed_point a, struct fixed_point b) {
    struct fixed_point fp;
    fp.base = (int64_t)a.base * F / b.base;
    return fp;
}

/* fp + int */
struct fixed_point add_int(struct fixed_point a, int b) {
    struct fixed_point fp;
    fp.base = a.base + b * F;
    return fp;
}

/* fp - int */
struct fixed_point sub_int(struct fixed_point a, int b) {
    struct fixed_point fp;
    fp.base = a.base - b * F;
    return fp;
}

/* fp * int */
struct fixed_point mult_int(struct fixed_point a, int b) {
    struct fixed_point fp;
    fp.base = a.base * b;
    return fp;
}

/* fp / int */
struct fixed_point div_int(struct fixed_point a, int b) {
    struct fixed_point fp;
    fp.base = a.base / b;
    return fp;
}