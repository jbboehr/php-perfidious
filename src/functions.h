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

#include <stdbool.h>
#include "Zend/zend_API.h"

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_get_pmu_info_arginfo, false, 1, PerfExt\\PmuInfo, false)
    ZEND_ARG_TYPE_INFO(false, pmu, IS_LONG, false)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_global_handle_arginfo, false, 0, PerfExt\\Handle, false)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO(perfidious_list_pmus_arginfo, IS_ARRAY, false)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO(perfidious_list_pmu_events_arginfo, IS_ARRAY, false)
    ZEND_ARG_TYPE_INFO(false, pmu, IS_LONG, false)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_open_arginfo, false, 1, PerfExt\\Handle, false)
    ZEND_ARG_TYPE_INFO(false, event_names, IS_ARRAY, false)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_INFO_EX(perfidious_request_handle_arginfo, false, 0, PerfExt\\Handle, true)
ZEND_END_ARG_INFO()

PERFIDIOUS_LOCAL extern ZEND_FUNCTION(perfidious_get_pmu_info);
PERFIDIOUS_LOCAL extern ZEND_FUNCTION(perfidious_global_handle);
PERFIDIOUS_LOCAL extern ZEND_FUNCTION(perfidious_list_pmus);
PERFIDIOUS_LOCAL extern ZEND_FUNCTION(perfidious_list_pmu_events);
PERFIDIOUS_LOCAL extern ZEND_FUNCTION(perfidious_open);
PERFIDIOUS_LOCAL extern ZEND_FUNCTION(perfidious_request_handle);

#endif /* PHP_PERF_FUNCTIONS_H */
