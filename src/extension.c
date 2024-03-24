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

#include <stdio.h>
#include <string.h>

#include "Zend/zend_API.h"
#include "Zend/zend_constants.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_ini.h"
#include "Zend/zend_modules.h"
#include "Zend/zend_operators.h"
#include "main/php.h"
#include "main/php_ini.h"
#include "main/SAPI.h"
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"

#include "php_perf.h"

ZEND_DECLARE_MODULE_GLOBALS(perf);

PHP_INI_BEGIN()
PHP_INI_END()

static PHP_RINIT_FUNCTION(perf)
{
#if defined(COMPILE_DL_PERF) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    return SUCCESS;
}

static PHP_MINIT_FUNCTION(perf)
{
    int flags = CONST_CS | CONST_PERSISTENT;
    zend_string *tmp;

    REGISTER_INI_ENTRIES();

    // Register constants
    REGISTER_STRING_CONSTANT("PerfExt\\VERSION", (char *) PHP_PERF_VERSION, flags);

    return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(perf)
{
    UNREGISTER_INI_ENTRIES();

    return SUCCESS;
}

static PHP_MINFO_FUNCTION(perf)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "Version", PHP_PERF_VERSION);
    php_info_print_table_row(2, "Released", PHP_PERF_RELEASE);
    php_info_print_table_row(2, "Authors", PHP_PERF_AUTHORS);
    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}

static PHP_GINIT_FUNCTION(perf)
{
#if defined(COMPILE_DL_PERF) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    memset(perf_globals, 0, sizeof(zend_perf_globals));
}

// clang-format off
const zend_function_entry perf_functions[] = {
    PHP_FE_END
};
// clang-format on

static const zend_module_dep perf_deps[] = {
    {"opcache", NULL, NULL, MODULE_DEP_OPTIONAL},
    ZEND_MOD_END,
};

zend_module_entry perf_module_entry = {
    STANDARD_MODULE_HEADER_EX,
    NULL,
    perf_deps,                /* Deps */
    PHP_PERF_NAME,            /* Name */
    perf_functions,           /* Functions */
    PHP_MINIT(perf),          /* MINIT */
    PHP_MSHUTDOWN(perf),      /* MSHUTDOWN */
    PHP_RINIT(perf),          /* RINIT */
    NULL,                      /* RSHUTDOWN */
    PHP_MINFO(perf),          /* MINFO */
    PHP_PERF_VERSION,         /* Version */
    PHP_MODULE_GLOBALS(perf), /* Globals */
    PHP_GINIT(perf),          /* GINIT */
    NULL,
    NULL,
    STANDARD_MODULE_PROPERTIES_EX,
};

#ifdef COMPILE_DL_PERF
#if defined(ZTS)
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(perf) // Common for all PHP extensions which are build as shared modules
#endif
