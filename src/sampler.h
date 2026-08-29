/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
 */

#ifndef PERFIDIOUS_SAMPLER_H
#define PERFIDIOUS_SAMPLER_H

#include <stdint.h>

#include "Zend/zend_types.h"

#include "php_perfidious.h"

PERFIDIOUS_LOCAL extern zend_class_entry *perfidious_metric_ce;
PERFIDIOUS_LOCAL extern zend_class_entry *perfidious_scope_ce;

enum perfidious_metric_id
{
    PERFIDIOUS_METRIC_CPU_TIME = 0,
    PERFIDIOUS_METRIC_PAGE_FAULTS = 1,
    PERFIDIOUS_METRIC_CONTEXT_SWITCHES = 2,
    PERFIDIOUS_METRIC_CPU_CYCLES = 3,
    PERFIDIOUS_METRIC_INSTRUCTIONS = 4,
    PERFIDIOUS_METRIC_COUNT = 5,
};

#define PERFIDIOUS_METRIC_MASK(metric) (((uint32_t) 1) << (metric))
#define PERFIDIOUS_METRIC_CPU_TIME_MASK PERFIDIOUS_METRIC_MASK(PERFIDIOUS_METRIC_CPU_TIME)
#define PERFIDIOUS_METRIC_PAGE_FAULTS_MASK PERFIDIOUS_METRIC_MASK(PERFIDIOUS_METRIC_PAGE_FAULTS)
#define PERFIDIOUS_METRIC_CONTEXT_SWITCHES_MASK PERFIDIOUS_METRIC_MASK(PERFIDIOUS_METRIC_CONTEXT_SWITCHES)
#define PERFIDIOUS_METRIC_CPU_CYCLES_MASK PERFIDIOUS_METRIC_MASK(PERFIDIOUS_METRIC_CPU_CYCLES)

enum perfidious_scope_id
{
    PERFIDIOUS_SCOPE_CURRENT_PROCESS = 0,
    PERFIDIOUS_SCOPE_CURRENT_THREAD = 1,
};

struct perfidious_platform_sampler;

struct perfidious_sampler_snapshot
{
    uint64_t values[PERFIDIOUS_METRIC_COUNT];
};

PERFIDIOUS_LOCAL zend_result perfidious_platform_sampler_supported_metrics(
    uint32_t requested_metrics, enum perfidious_scope_id scope, uint32_t *supported_metrics
);

PERFIDIOUS_LOCAL zend_result perfidious_platform_sampler_open(
    uint32_t metrics, enum perfidious_scope_id scope, struct perfidious_platform_sampler **sampler
);

PERFIDIOUS_LOCAL zend_result perfidious_platform_sampler_read(
    struct perfidious_platform_sampler *sampler, struct perfidious_sampler_snapshot *snapshot
);

PERFIDIOUS_LOCAL void perfidious_platform_sampler_close(struct perfidious_platform_sampler *sampler);

PERFIDIOUS_LOCAL void perfidious_sampler_minit(void);

#endif /* PERFIDIOUS_SAMPLER_H */
