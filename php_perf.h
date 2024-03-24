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

#ifndef PHP_PERF_H
#define PHP_PERF_H

#include "main/php.h"

#define PHP_PERF_NAME "perf"
#define PHP_PERF_VERSION "0.1.0"
#define PHP_PERF_RELEASE "2024-01-27"
#define PHP_PERF_AUTHORS "John Boehr <jbboehr@gmail.com> (lead)"

#if (__GNUC__ >= 4) || defined(__clang__) || defined(HAVE_FUNC_ATTRIBUTE_VISIBILITY)
#define PERF_PUBLIC __attribute__((visibility("default")))
#define PERF_LOCAL __attribute__((visibility("hidden")))
#elif defined(PHP_WIN32) && defined(PERF_EXPORTS)
#define PERF_PUBLIC __declspec(dllexport)
#define PERF_LOCAL
#else
#define PERF_PUBLIC
#define PERF_LOCAL
#endif

extern zend_module_entry perf_module_entry;
#define phpext_perf_ptr &perf_module_entry

#if defined(ZTS) && ZTS
#include "TSRM.h"
#endif

#if defined(ZTS) && ZTS
#define PERF_G(v) TSRMG(perf_globals_id, zend_perf_globals *, v)
#else
#define PERF_G(v) (perf_globals.v)
#endif

#if defined(ZTS) && defined(COMPILE_DL_PERF)
ZEND_TSRMLS_CACHE_EXTERN();
#endif

ZEND_BEGIN_MODULE_GLOBALS(perf)
ZEND_END_MODULE_GLOBALS(perf)

ZEND_EXTERN_MODULE_GLOBALS(perf);

#endif /* PHP_PERF_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: fdm=marker
 * vim: et sw=4 ts=4
 */
