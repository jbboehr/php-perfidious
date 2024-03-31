
m4_include(m4/ax_append_compile_flags.m4)
m4_include(m4/ax_append_flag.m4)
m4_include(m4/ax_append_link_flags.m4)
m4_include(m4/ax_cflags_warn_all.m4)
m4_include(m4/ax_check_link_flag.m4)
m4_include(m4/ax_compiler_flags.m4)
m4_include(m4/ax_compiler_flags_cflags.m4)
m4_include(m4/ax_compiler_flags_gir.m4)
m4_include(m4/ax_compiler_flags_ldflags.m4)
m4_include(m4/ax_compiler_vendor.m4)
m4_include(m4/ax_is_release.m4)
m4_include(m4/ax_prepend_flag.m4)
m4_include(m4/ax_require_defined.m4)

m4_define(AM_LDFLAGS, [LDFLAGS])

PHP_ARG_ENABLE(perf,     whether to enable vyrtue support,
[AS_HELP_STRING([--enable-perf], [Enable perf support])])

PHP_ARG_ENABLE(perf-debug, whether to enable perf debug support,
[AS_HELP_STRING([--enable-perf-debug], [Enable perf debug support])], [no], [no])

AC_DEFUN([PHP_PERF_ADD_SOURCES], [
  PHP_PERF_SOURCES="$PHP_PERF_SOURCES $1"
])

if test "$PHP_PERF" != "no"; then
    AX_IS_RELEASE([git-directory])
    AX_CFLAGS_WARN_ALL([WARN_CFLAGS])

    # a lot of these are in PHP headers...
    AX_COMPILER_FLAGS([WARN_CFLAGS],[WARN_LDFLAGS],,,[ \
        -Wno-undef -Wno-error=undef \
        -Wno-redundant-decls -Wno-error=redundant-decls \
        -Wno-missing-include-dirs -Wno-error=missing-include-dirs \
        -Wno-declaration-after-statement -Wno-error=declaration-after-statement \
        -Wno-shadow -Wno-error=shadow \
        -Wno-missing-prototypes -Wno-error=missing-prototypes \
        -Wno-missing-declarations -Wno-error=missing-declarations \
        -Wno-cast-align -Wno-error=cast-align \
    ])
    
    CFLAGS="$WARN_CFLAGS $CFLAGS"
    LDFLAGS="$WARN_LDFLAGS $LDFLAGS"

    if test "$PHP_PERF_DEBUG" == "yes"; then
        AC_DEFINE([PERF_DEBUG], [1], [Enable vyrtue debug support])
    fi

    PHP_ADD_LIBRARY(pfm, , PERF_SHARED_LIBADD)

    PHP_PERF_ADD_SOURCES([
        src/extension.c
        src/functions.c
        src/pmu_enum.c
        src/handle.c
    ])

    PHP_ADD_BUILD_DIR(src)
    PHP_INSTALL_HEADERS([ext/perf], [php_perf.h])
    PHP_NEW_EXTENSION(perf, $PHP_PERF_SOURCES, $ext_shared, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
    PHP_SUBST(PERF_SHARED_LIBADD)
fi
