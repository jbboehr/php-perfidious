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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "Zend/zend_API.h"
#include "Zend/zend_constants.h"
#include "Zend/zend_modules.h"
#include "main/php.h"
#include "ext/standard/info.h"

#include "php_perfidious.h"

ZEND_DECLARE_MODULE_GLOBALS(perfidious);

PERFIDIOUS_LOCAL void perfidious_exceptions_minit(void);
PERFIDIOUS_LOCAL void perfidious_read_result_minit(void);

static PHP_RINIT_FUNCTION(perfidious)
{
#if defined(COMPILE_DL_PERFIDIOUS) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    return SUCCESS;
}

static const char *PERFIDIOUS_MOTD =
    "Think not that I am come to send peace on earth: I came not to send peace, but a sword. Matthew 10:34";

static PHP_MINIT_FUNCTION(perfidious)
{
    const int flags = CONST_CS | CONST_PERSISTENT;

    REGISTER_BOOL_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\DEBUG", false, flags);
    REGISTER_STRING_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\VERSION", (char *) PHP_PERFIDIOUS_VERSION, flags);
    REGISTER_LONG_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\OVERFLOW_THROW", PERFIDIOUS_OVERFLOW_THROW, flags);
    REGISTER_LONG_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\OVERFLOW_WARN", PERFIDIOUS_OVERFLOW_WARN, flags);
    REGISTER_LONG_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\OVERFLOW_SATURATE", PERFIDIOUS_OVERFLOW_SATURATE, flags);
    REGISTER_LONG_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\OVERFLOW_WRAP", PERFIDIOUS_OVERFLOW_WRAP, flags);
    REGISTER_STRING_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\MOTD", (char *) PERFIDIOUS_MOTD, flags);

    perfidious_exceptions_minit();
    perfidious_read_result_minit();

    return SUCCESS;
}

static PHP_MINFO_FUNCTION(perfidious)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "Version", PHP_PERFIDIOUS_VERSION);
    php_info_print_table_row(2, "Released", PHP_PERFIDIOUS_RELEASE);
    php_info_print_table_row(2, "Authors", PHP_PERFIDIOUS_AUTHORS);
    php_info_print_table_end();

    php_info_print_box_start(0);
    PUTS(PERFIDIOUS_MOTD);
    php_info_print_box_end();
}

static PHP_GINIT_FUNCTION(perfidious)
{
    memset(perfidious_globals, 0, sizeof(zend_perfidious_globals));
    perfidious_globals->error_mode = PERFIDIOUS_ERROR_MODE_THROW;
}

static const zend_module_dep perfidious_deps[] = {
    {"spl", NULL, NULL, MODULE_DEP_REQUIRED},
    ZEND_MOD_END,
};

zend_module_entry perfidious_module_entry = {
    STANDARD_MODULE_HEADER_EX,
    NULL,
    perfidious_deps,
    PHP_PERFIDIOUS_NAME,
    NULL,
    PHP_MINIT(perfidious),
    NULL,
    PHP_RINIT(perfidious),
    NULL,
    PHP_MINFO(perfidious),
    PHP_PERFIDIOUS_VERSION,
    PHP_MODULE_GLOBALS(perfidious),
    PHP_GINIT(perfidious),
    NULL,
    NULL,
    STANDARD_MODULE_PROPERTIES_EX,
};

#ifdef COMPILE_DL_PERFIDIOUS
#if defined(ZTS)
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_DLEXPORT zend_module_entry *get_module(void);
ZEND_GET_MODULE(perfidious)
#endif
