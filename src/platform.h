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

#ifndef PERFIDIOUS_PLATFORM_H
#define PERFIDIOUS_PLATFORM_H

#include "Zend/zend_API.h"
#include "Zend/zend_modules.h"
#include "main/php.h"

#include "php_perfidious.h"

#if defined(PERFIDIOUS_PLATFORM_LINUX)
PERFIDIOUS_LOCAL extern const zend_function_entry perfidious_functions[];
#define PERFIDIOUS_PLATFORM_FUNCTIONS perfidious_functions
#elif defined(PERFIDIOUS_PLATFORM_WINDOWS)
PERFIDIOUS_LOCAL extern const zend_function_entry perfidious_windows_functions[];
#define PERFIDIOUS_PLATFORM_FUNCTIONS perfidious_windows_functions
#elif defined(PERFIDIOUS_PLATFORM_DARWIN)
PERFIDIOUS_LOCAL extern const zend_function_entry perfidious_darwin_functions[];
#define PERFIDIOUS_PLATFORM_FUNCTIONS perfidious_darwin_functions
#else
#error "Unsupported perfidious platform"
#endif

PERFIDIOUS_LOCAL PHP_MINIT_FUNCTION(perfidious_platform);
PERFIDIOUS_LOCAL PHP_MSHUTDOWN_FUNCTION(perfidious_platform);
PERFIDIOUS_LOCAL PHP_RINIT_FUNCTION(perfidious_platform);
PERFIDIOUS_LOCAL PHP_RSHUTDOWN_FUNCTION(perfidious_platform);
PERFIDIOUS_LOCAL PHP_MINFO_FUNCTION(perfidious_platform);

#endif /* PERFIDIOUS_PLATFORM_H */
