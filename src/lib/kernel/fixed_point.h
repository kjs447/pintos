#ifndef __LIB_KERNEL_FIXED_POINT_H
#define __LIB_KERNEL_FIXED_POINT_H

/* 17.14 fixed point */
struct fixed_point {
    int base;
};

/* convert fixed point <-> integer */
struct fixed_point to_fp(int);
int to_int(struct fixed_point);

/* fixed point operation. */
struct fixed_point add(struct fixed_point, struct fixed_point);
struct fixed_point sub(struct fixed_point, struct fixed_point);
struct fixed_point mult(struct fixed_point, struct fixed_point);
struct fixed_point div(struct fixed_point, struct fixed_point);

struct fixed_point add_int(struct fixed_point, int);
struct fixed_point sub_int(struct fixed_point, int);
struct fixed_point mult_int(struct fixed_point, int);
struct fixed_point div_int(struct fixed_point, int);

#endif /* lib/kernel/fixed_point.h */