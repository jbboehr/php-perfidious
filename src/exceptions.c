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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Zend/zend_API.h>
#include <Zend/zend_enum.h>
#include <ext/spl/spl_exceptions.h>
#include "php_perf.h"

PERFIDIOUS_PUBLIC zend_class_entry *perfidious_pmu_not_found_exception_ce;

static zend_class_entry *register_class_PmuNotFoundException(void)
{
    zend_class_entry ce;
    zend_class_entry *class_entry;

    INIT_CLASS_ENTRY(ce, PHP_PERF_NAMESPACE "\\PmuNotFoundException", NULL);
    class_entry = zend_register_internal_class_ex(&ce, spl_ce_InvalidArgumentException);

    return class_entry;
}

PERFIDIOUS_LOCAL zend_result perfidious_exceptions_minit(void)
{
    perfidious_pmu_not_found_exception_ce = register_class_PmuNotFoundException();

    return SUCCESS;
}
