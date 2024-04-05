
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

PHP_ARG_ENABLE(perfidious,     whether to enable perfidious,
[AS_HELP_STRING([--enable-perfidious], [Enable perfidious])])

PHP_ARG_ENABLE(perfidious-debug, whether to enable perfidious debug support,
[AS_HELP_STRING([--enable-perfidious-debug], [Enable perfidious debug support])], [no], [no])

AC_DEFUN([PHP_PERFIDIOUS_ADD_SOURCES], [
  PHP_PERFIDIOUS_SOURCES="$PHP_PERFIDIOUS_SOURCES $1"
])

if test "$PHP_PERFIDIOUS" != "no"; then
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

    AC_CHECK_SIZEOF(pid_t)
    AC_CHECK_SIZEOF(uint64_t)

    CFLAGS="$WARN_CFLAGS $CFLAGS"
    LDFLAGS="$WARN_LDFLAGS $LDFLAGS"

    if test "$PHP_PERFIDIOUS_DEBUG" == "yes"; then
        AC_DEFINE([PERFIDIOUS_DEBUG], [1], [Enable vyrtue debug support])
    else
        AC_DEFINE([NDEBUG], [1], [Disable debug support])
    fi

    PHP_ADD_LIBRARY(cap, , PERFIDIOUS_SHARED_LIBADD)
    PHP_ADD_LIBRARY(pfm, , PERFIDIOUS_SHARED_LIBADD)

    PHP_PERFIDIOUS_ADD_SOURCES([
        src/extension.c
        src/exceptions.c
        src/functions.c
        src/handle.c
        src/pmu_event_info.c
        src/pmu_info.c
        src/read_result.c
    ])

    PHP_ADD_BUILD_DIR(src)
    PHP_INSTALL_HEADERS([ext/perfidious], [php_perfidious.h])
    PHP_NEW_EXTENSION(perfidious, $PHP_PERFIDIOUS_SOURCES, $ext_shared, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
    PHP_ADD_EXTENSION_DEP(perfidious, spl, false)
    PHP_ADD_EXTENSION_DEP(perfidious, opcache, true)
    PHP_SUBST(PERFIDIOUS_SHARED_LIBADD)
fi
