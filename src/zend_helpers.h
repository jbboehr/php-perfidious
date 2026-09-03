/**
 * Copyright (c) anno Domini nostri Jesu Christi MMXXIV John Boehr & contributors
 *
 * SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
 */

#ifndef PERFIDIOUS_ZEND_HELPERS_H
#define PERFIDIOUS_ZEND_HELPERS_H

#include "main/php.h"
#include "Zend/zend_API.h"

#if PHP_VERSION_ID >= 80400
#define PERFIDIOUS_RAW_FENTRY(zend_name, name, arg_info, flags)                                                        \
    ZEND_RAW_FENTRY(zend_name, name, arg_info, flags, NULL, NULL)
#else
#define PERFIDIOUS_RAW_FENTRY(zend_name, name, arg_info, flags) ZEND_RAW_FENTRY(zend_name, name, arg_info, flags)
#endif

static zend_always_inline void perfidious_declare_readonly_property_ex(
    zend_class_entry *class_entry, zend_string *property_name, zend_type type
)
{
    zval default_value;

    ZVAL_UNDEF(&default_value);
    zend_declare_typed_property(
        class_entry, property_name, &default_value, ZEND_ACC_PUBLIC | ZEND_ACC_READONLY, NULL, type
    );
}

static zend_always_inline void perfidious_declare_readonly_property(
    zend_class_entry *class_entry, const char *name, size_t name_length, zend_type type
)
{
    perfidious_declare_readonly_property_ex(
        class_entry, zend_string_init_interned(name, name_length, true), type
    );
}

#define PERFIDIOUS_DECLARE_READONLY_PROPERTY(class_entry, name, type_mask)                                             \
    perfidious_declare_readonly_property(                                                                              \
        class_entry, ZEND_STRL(name), (zend_type) ZEND_TYPE_INIT_MASK(type_mask)                                       \
    )

#endif /* PERFIDIOUS_ZEND_HELPERS_H */
