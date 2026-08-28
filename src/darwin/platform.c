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

#include "main/php.h"

#include "php_perfidious.h"
#include "../platform.h"

PERFIDIOUS_LOCAL PHP_MINIT_FUNCTION(perfidious_platform)
{
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

PERFIDIOUS_LOCAL PHP_RSHUTDOWN_FUNCTION(perfidious_platform)
{
    return SUCCESS;
}

PERFIDIOUS_LOCAL PHP_MINFO_FUNCTION(perfidious_platform)
{
}
