/**
 * Copyright (C) 2024 John Boehr & contributors
 *
 * This file is part of php-perf.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PHP_PERF_FUNCTIONS_H
#define PHP_PERF_FUNCTIONS_H

#include "Zend/zend_API.h"

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perf_get_pmu_info_arginfo, 0, 0, PerfExt\\PmuInfo, 0)
    ZEND_ARG_TYPE_INFO(0, pmu, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO(perf_list_pmus_arginfo, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO(perf_list_pmu_events_arginfo, IS_ARRAY, 0)
    ZEND_ARG_TYPE_INFO(0, pmu, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perf_open_arginfo, 0, 0, PerfExt\\Handle, 0)
    ZEND_ARG_TYPE_INFO(0, event_names, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

PERF_LOCAL extern ZEND_FUNCTION(perf_get_pmu_info);
PERF_LOCAL extern ZEND_FUNCTION(perf_list_pmus);
PERF_LOCAL extern ZEND_FUNCTION(perf_list_pmu_events);
PERF_LOCAL extern ZEND_FUNCTION(perf_open);

#endif /* PHP_PERF_FUNCTIONS_H */
