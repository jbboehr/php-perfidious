/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
 */

#ifndef PERFIDIOUS_DARWIN_RESOURCE_USAGE_H
#define PERFIDIOUS_DARWIN_RESOURCE_USAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "Zend/zend_types.h"

#include "php_perfidious.h"

struct perfidious_darwin_thread_resource_usage
{
    uint64_t user_time_ns;
    uint64_t system_time_ns;
    uint64_t instructions;
    uint64_t cycles;
};

PERFIDIOUS_LOCAL zend_result perfidious_darwin_get_current_thread_id(uint64_t *thread_id);

PERFIDIOUS_LOCAL bool perfidious_darwin_mach_time_to_ns(uint64_t value, uint64_t *result);

PERFIDIOUS_LOCAL zend_result
perfidious_darwin_read_current_thread_resource_usage(struct perfidious_darwin_thread_resource_usage *usage);

#endif /* PERFIDIOUS_DARWIN_RESOURCE_USAGE_H */
