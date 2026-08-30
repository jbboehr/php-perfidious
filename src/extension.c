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

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Zend/zend_API.h"
#include "Zend/zend_constants.h"
#include "Zend/zend_modules.h"
#include "main/php.h"
#include "ext/standard/info.h"

#include "php_perfidious.h"
#include "platform.h"
#include "sampler.h"

ZEND_DECLARE_MODULE_GLOBALS(perfidious);

PERFIDIOUS_LOCAL void perfidious_exceptions_minit(void);
PERFIDIOUS_LOCAL void perfidious_read_result_minit(void);

#if defined(PERFIDIOUS_PLATFORM_LINUX)
const char *PERFIDIOUS_MOTD =
#else
static const char *PERFIDIOUS_MOTD =
#endif
    "Think not that I am come to send peace on earth: I came not to send peace, but a sword. Matthew 10:34";

static PHP_MINIT_FUNCTION(perfidious)
{
    const int flags = CONST_CS | CONST_PERSISTENT;

    PERFIDIOUS_G(error_mode) = PERFIDIOUS_ERROR_MODE_WARNING;

#if defined(PERFIDIOUS_PLATFORM_LINUX) && defined(PERFIDIOUS_DEBUG)
    REGISTER_BOOL_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\DEBUG", (zend_bool) PERFIDIOUS_DEBUG, flags);
#else
    REGISTER_BOOL_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\DEBUG", false, flags);
#endif
    REGISTER_STRING_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\VERSION", (char *) PHP_PERFIDIOUS_VERSION, flags);
#if defined(PERFIDIOUS_PLATFORM_LINUX) && defined(PERFIDIOUS_DEBUG)
    do {
        char buf[128];
        snprintf(buf, sizeof(buf), "%" PRIu64, UINT64_MAX);
        REGISTER_STRING_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\UINT64_MAX", buf, flags);
    } while (false);
#endif

    REGISTER_STRING_CONSTANT(PHP_PERFIDIOUS_NAMESPACE "\\MOTD", (char *) PERFIDIOUS_MOTD, flags);

    perfidious_exceptions_minit();
    perfidious_read_result_minit();
    perfidious_sampler_minit();

    if (UNEXPECTED(FAILURE == PHP_MINIT(perfidious_platform)(INIT_FUNC_ARGS_PASSTHRU))) {
        return FAILURE;
    }

    PERFIDIOUS_G(error_mode) = PERFIDIOUS_ERROR_MODE_THROW;

    return SUCCESS;
}

static PHP_MSHUTDOWN_FUNCTION(perfidious)
{
    PERFIDIOUS_G(error_mode) = PERFIDIOUS_ERROR_MODE_WARNING;

    return PHP_MSHUTDOWN(perfidious_platform)(SHUTDOWN_FUNC_ARGS_PASSTHRU);
}

static PHP_RINIT_FUNCTION(perfidious)
{
#if defined(COMPILE_DL_PERFIDIOUS) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    return PHP_RINIT(perfidious_platform)(INIT_FUNC_ARGS_PASSTHRU);
}

static PHP_RSHUTDOWN_FUNCTION(perfidious)
{
    return PHP_RSHUTDOWN(perfidious_platform)(SHUTDOWN_FUNC_ARGS_PASSTHRU);
}

static PHP_MINFO_FUNCTION(perfidious)
{
    php_info_print_table_start();
    php_info_print_table_row(2, "Version", PHP_PERFIDIOUS_VERSION);
    php_info_print_table_row(2, "Released", PHP_PERFIDIOUS_RELEASE);
    php_info_print_table_row(2, "Authors", PHP_PERFIDIOUS_AUTHORS);
    php_info_print_table_end();

    PHP_MINFO(perfidious_platform)(ZEND_MODULE_INFO_FUNC_ARGS_PASSTHRU);

    php_info_print_box_start(0);
    PUTS(PERFIDIOUS_MOTD);
    php_info_print_box_end();
}

static PHP_GINIT_FUNCTION(perfidious)
{
#if defined(COMPILE_DL_PERFIDIOUS) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    memset(perfidious_globals, 0, sizeof(zend_perfidious_globals));
    perfidious_globals->error_mode = PERFIDIOUS_ERROR_MODE_THROW;
}

static const zend_module_dep perfidious_deps[] = {
    {"spl",     NULL, NULL, MODULE_DEP_REQUIRED},
#if defined(PERFIDIOUS_PLATFORM_LINUX)
    {"opcache", NULL, NULL, MODULE_DEP_OPTIONAL},
#endif
    ZEND_MOD_END,
};

zend_module_entry perfidious_module_entry = {
    STANDARD_MODULE_HEADER_EX,
    NULL,
    perfidious_deps,                /* Deps */
    PHP_PERFIDIOUS_NAME,            /* Name */
    PERFIDIOUS_PLATFORM_FUNCTIONS,  /* Functions */
    PHP_MINIT(perfidious),          /* MINIT */
    PHP_MSHUTDOWN(perfidious),      /* MSHUTDOWN */
    PHP_RINIT(perfidious),          /* RINIT */
    PHP_RSHUTDOWN(perfidious),      /* RSHUTDOWN */
    PHP_MINFO(perfidious),          /* MINFO */
    PHP_PERFIDIOUS_VERSION,         /* Version */
    PHP_MODULE_GLOBALS(perfidious), /* Globals */
    PHP_GINIT(perfidious),          /* GINIT */
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
