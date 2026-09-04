/**
 * Negative-control fixture for tests/valgrind/libpfm-initialize.supp.
 *
 * This deliberately builds an unrelated PHP extension whose MINIT leaks
 * memory through four frames beneath zend_startup_module_ex.  A suppression
 * scoped to libpfm must not hide this allocation.
 *
 * Compile from the repository root with optimization enabled so the Nix C
 * wrapper's fortify checks remain valid, while disabling sibling-call
 * optimization so the test stack remains observable:
 *
 *   cc -shared -fPIC -g -O1 -fno-omit-frame-pointer \
 *      -fno-optimize-sibling-calls -Wall -Wextra -Werror \
 *      $(php-config --includes) tests/valgrind/libpfm-suppression-scope.c \
 *      -o /tmp/perfidious-libpfm-suppression-scope.so
 *
 * The following command must report the 73-byte definite leak and exit 41:
 *
 *   valgrind --quiet --keep-debuginfo=yes --leak-check=full \
 *      --show-leak-kinds=definite --errors-for-leak-kinds=definite \
 *      --error-exitcode=41 \
 *      --suppressions=tests/valgrind/libpfm-initialize.supp php -n \
 *      -d extension=/tmp/perfidious-libpfm-suppression-scope.so -r ''
 */

#include <stdlib.h>

#include "main/php.h"

__attribute__((noinline)) static void unrelated_allocate(void)
{
    void *leaked = calloc(1, 73);
    __asm__ volatile("" : : "r"(leaked) : "memory");
}

__attribute__((noinline)) static void unrelated_two(void)
{
    unrelated_allocate();
}

__attribute__((noinline)) static void unrelated_one(void)
{
    unrelated_two();
}

PHP_MINIT_FUNCTION(libpfm_suppression_scope)
{
    (void) type;
    (void) module_number;
    unrelated_one();
    return SUCCESS;
}

zend_module_entry libpfm_suppression_scope_module_entry = {
    STANDARD_MODULE_HEADER,
    "libpfm_suppression_scope",
    NULL,
    PHP_MINIT(libpfm_suppression_scope),
    NULL,
    NULL,
    NULL,
    NULL,
    "test-only",
    STANDARD_MODULE_PROPERTIES
};

ZEND_GET_MODULE(libpfm_suppression_scope)
