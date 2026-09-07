/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License version 3,
 * as published by the Free Software Foundation, together with the Romic
 * Exception (an additional permission under section 7 of that license).
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * and the Romic Exception along with this program.  If not, see
 * <http://www.gnu.org/licenses/> and the LICENSE_EXCEPTION file.
 */

#ifndef PERFIDIOUS_PRIVATE_H
#define PERFIDIOUS_PRIVATE_H

#include <stdbool.h>
#include <inttypes.h>
#include <limits.h>
#include <Zend/zend_API.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_long.h>
#include <Zend/zend_portability.h>
#include "php_perfidious.h"

// interned strings for pmu_infp
PERFIDIOUS_LOCAL extern zend_string *PERFIDIOUS_INTERNED_NAME;
PERFIDIOUS_LOCAL extern zend_string *PERFIDIOUS_INTERNED_DESC;
PERFIDIOUS_LOCAL extern zend_string *PERFIDIOUS_INTERNED_PMU;
PERFIDIOUS_LOCAL extern zend_string *PERFIDIOUS_INTERNED_TYPE;
PERFIDIOUS_LOCAL extern zend_string *PERFIDIOUS_INTERNED_NEVENTS;
PERFIDIOUS_LOCAL extern zend_string *PERFIDIOUS_INTERNED_IS_PRESENT;

// interned strings for pmu_event_infp
PERFIDIOUS_LOCAL extern zend_string *PERFIDIOUS_INTERNED_EQUIV;

// interned strings for resd_result
PERFIDIOUS_LOCAL extern zend_string *PERFIDIOUS_INTERNED_TIME_ENABLED;
PERFIDIOUS_LOCAL extern zend_string *PERFIDIOUS_INTERNED_TIME_RUNNING;
PERFIDIOUS_LOCAL extern zend_string *PERFIDIOUS_INTERNED_VALUES;

#define PERFIDIOUS_ASSERT_RETURN(expr)                                                                                 \
    if (UNEXPECTED(!(expr))) {                                                                                         \
        return FAILURE;                                                                                                \
    }

#define PERFIDIOUS_ASSERT_RETURN_EX(expr, block)                                                                       \
    if (UNEXPECTED(!(expr))) {                                                                                         \
        do                                                                                                             \
            block while (false);                                                                                       \
        return FAILURE;                                                                                                \
    }

static inline bool perfidious_uint64_t_to_zend_long(uint64_t from, zend_long *restrict to)
{
#if SIZEOF_UINT64_T >= SIZEOF_ZEND_LONG
    if (UNEXPECTED(from > ZEND_LONG_MAX)) {
        zend_throw_exception_ex(
            perfidious_overflow_exception_ce,
            0,
            "value too large: %" PRIu64 " > %" ZEND_LONG_FMT_SPEC,
            from,
            ZEND_LONG_MAX
        );
        return false;
    }
#endif

    *to = (zend_long) from;
    return true;
}

static inline bool perfidious_zend_long_to_pid_t(zend_long from, pid_t *restrict to)
{
#if SIZEOF_ZEND_LONG > SIZEOF_PID_T
    const zend_long PID_MAX = (((zend_long) 1) << ((SIZEOF_PID_T * CHAR_BIT) - 1)) - 1;
    const zend_long PID_MIN = -PID_MAX - 1;
    if (UNEXPECTED(from > PID_MAX)) {
        zend_throw_exception_ex(
            perfidious_overflow_exception_ce,
            0,
            "pid too large: %" ZEND_LONG_FMT_SPEC " > %" ZEND_LONG_FMT_SPEC,
            from,
            PID_MAX
        );
        return false;
    }
    if (UNEXPECTED(from < PID_MIN)) {
        zend_throw_exception_ex(
            perfidious_overflow_exception_ce,
            0,
            "pid too small: %" ZEND_LONG_FMT_SPEC " < %" ZEND_LONG_FMT_SPEC,
            from,
            PID_MIN
        );
        return false;
    }
#endif

    *to = (pid_t) from;
    return true;
}

ZEND_ATTRIBUTE_UNUSED
ZEND_ATTRIBUTE_FORMAT(printf, 4, 5)
static void perfidious_error_set(
    struct perfidious_error *error, zend_class_entry *exception_ce, zend_long code, const char *format, ...
)
{
    error->exception_ce = exception_ce;
    error->code = code;
    va_list args;
    va_start(args, format);
    int bytes = vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
    if (bytes < 0) {
        error->message[0] = '\0';
    }
}

ZEND_ATTRIBUTE_UNUSED
ZEND_ATTRIBUTE_FORMAT(printf, 3, 4)
static void
perfidious_error_helper(zend_class_entry *restrict exception_ce, zend_long code, const char *restrict format, ...)
{
    char buffer[512];
    int bytes = 0;
    va_list args;

    va_start(args, format);
    bytes = vslprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    int len = bytes < 0 ? 0 : bytes;

    switch (PERFIDIOUS_G(error_mode)) {
        case PERFIDIOUS_ERROR_MODE_WARNING:
            php_error_docref(NULL, E_WARNING, "%.*s", len, buffer);
            break;
        default:
        case PERFIDIOUS_ERROR_MODE_THROW:
            zend_throw_exception_ex(exception_ce, code, "%.*s", len, buffer);
            break;
    }
}

static inline pfm_err_t perfidious_pfm_get_pmu_info(zend_long pmu, pfm_pmu_info_t *info)
{
    // Check the PHP integer before narrowing it to libpfm's PMU enum.
    if (UNEXPECTED(pmu < PFM_PMU_NONE || pmu >= PFM_PMU_MAX)) {
        return PFM_ERR_INVAL;
    }
    return pfm_get_pmu_info((pfm_pmu_t) pmu, info);
}

ZEND_COLD
PERFIDIOUS_LOCAL
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
zend_result perfidious_get_pmu_info(zend_long pmu, zval *restrict return_value, bool silent);

ZEND_COLD
PERFIDIOUS_LOCAL
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
zend_result perfidious_get_pmu_event_info(pfm_pmu_info_t *restrict pmu_info, zend_long idx, zval *restrict return_value);

#ifdef PERFIDIOUS_DEBUG
// exercises perfidious_pmu_event_info_ctor()'s name truncation with caller-controlled strings,
// since real libpfm event names are never long enough to hit the truncation path
ZEND_COLD
PERFIDIOUS_LOCAL
PERFIDIOUS_ATTR_NONNULL_ALL
PERFIDIOUS_ATTR_WARN_UNUSED_RESULT
zend_result perfidious_debug_pmu_event_info_ctor(
    zend_string *restrict pmu_name, zend_string *restrict event_name, zval *restrict return_value
);
#endif

#endif /* PERFIDIOUS_PRIVATE_H */
