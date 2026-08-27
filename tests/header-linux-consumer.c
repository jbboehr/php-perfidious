/**
 * Compile-only regression test for Linux consumers of the installed public header.
 */

#include "php_perfidious.h"

zend_class_entry **perfidious_consumer_handle_class = &perfidious_handle_ce;
zend_result (*perfidious_consumer_handle_reset)(struct perfidious_handle *) = perfidious_handle_reset;
