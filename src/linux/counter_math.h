/* Checked integer scaling shared by Linux counter consumers. */
#ifndef PERFIDIOUS_LINUX_COUNTER_MATH_H
#define PERFIDIOUS_LINUX_COUNTER_MATH_H

#include <stdint.h>

static inline bool perfidious_scale_uint64(uint64_t value, uint64_t multiplier, uint64_t divisor, uint64_t *result)
{
    ZEND_ASSERT(divisor != 0);

#if defined(__SIZEOF_INT128__)
    __extension__ typedef unsigned __int128 perfidious_uint128_t;
    perfidious_uint128_t scaled = (perfidious_uint128_t) value * multiplier / divisor;

    if (UNEXPECTED(scaled > UINT64_MAX)) {
        return false;
    }

    *result = (uint64_t) scaled;
    return true;
#else
    if (UNEXPECTED(value != 0 && multiplier > UINT64_MAX / value)) {
        return false;
    }

    *result = value * multiplier / divisor;
    return true;
#endif
}

#endif /* PERFIDIOUS_LINUX_COUNTER_MATH_H */
