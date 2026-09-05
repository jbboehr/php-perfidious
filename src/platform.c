/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "main/php.h"

#include "php_perfidious.h"
#include "platform.h"

#if defined(PERFIDIOUS_PLATFORM_DARWIN)
PERFIDIOUS_LOCAL void perfidious_darwin_minit(void);
#define perfidious_platform_backend_minit perfidious_darwin_minit
#elif defined(PERFIDIOUS_PLATFORM_WINDOWS)
PERFIDIOUS_LOCAL void perfidious_windows_minit(void);
#define perfidious_platform_backend_minit perfidious_windows_minit
#else
#error "src/platform.c requires the Darwin or Windows backend"
#endif

PERFIDIOUS_LOCAL PHP_MINIT_FUNCTION(perfidious_platform)
{
    perfidious_platform_backend_minit();

    return SUCCESS;
}

PERFIDIOUS_LOCAL PHP_MSHUTDOWN_FUNCTION(perfidious_platform)
{
    return SUCCESS;
}

PERFIDIOUS_LOCAL PHP_RINIT_FUNCTION(perfidious_platform)
{
    return SUCCESS;
}

PERFIDIOUS_LOCAL void perfidious_platform_globals_shutdown(zend_perfidious_globals *perfidious_globals)
{
}

PERFIDIOUS_LOCAL PHP_RSHUTDOWN_FUNCTION(perfidious_platform)
{
    return SUCCESS;
}

PERFIDIOUS_LOCAL PHP_MINFO_FUNCTION(perfidious_platform)
{
}
