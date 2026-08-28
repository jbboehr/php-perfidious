
m4_include(m4/ax_append_compile_flags.m4)
m4_include(m4/ax_append_flag.m4)
m4_include(m4/ax_append_link_flags.m4)
m4_include(m4/ax_cflags_warn_all.m4)
m4_include(m4/ax_check_link_flag.m4)
m4_include(m4/ax_compiler_flags.m4)
m4_include(m4/ax_compiler_flags_cflags.m4)
m4_include(m4/ax_compiler_flags_cxxflags.m4)
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

PHP_ARG_ENABLE(perfidious-coverage, whether to enable perfidious coverage support,
[AS_HELP_STRING([--enable-perfidious-coverage], [Enable perfidious coverage support])], [no], [no])

PHP_ARG_ENABLE(perfidious-sanitize, whether to build perfidious with ASan/UBSan,
[AS_HELP_STRING([--enable-perfidious-sanitize], [Build perfidious with ASan/UBSan])], [no], [no])

AC_DEFUN([PHP_PERFIDIOUS_ADD_SOURCES], [
  PHP_PERFIDIOUS_SOURCES="$PHP_PERFIDIOUS_SOURCES $1"
])

PHP_PERFIDIOUS_COMMON_SOURCES="
    src/extension.c
    src/exceptions.c
    src/read_result.c
"

PHP_PERFIDIOUS_LINUX_SOURCES="
    src/functions.c
    src/handle.c
    src/linux/platform.c
    src/pmu_event_info.c
    src/pmu_info.c
"

PHP_PERFIDIOUS_DARWIN_SOURCES="
    src/darwin/platform.c
"

if test "$PHP_PERFIDIOUS" != "no"; then
    AS_CASE([$host_os],
        [linux*], [
            AC_DEFINE(
                [PERFIDIOUS_PLATFORM_LINUX],
                [1],
                [Define to 1 when building the Linux perf_events backend]
            )
            PHP_ADD_LIBRARY(cap, , PERFIDIOUS_SHARED_LIBADD)
            PHP_ADD_LIBRARY(pfm, , PERFIDIOUS_SHARED_LIBADD)
            PHP_PERFIDIOUS_ADD_SOURCES([
                $PHP_PERFIDIOUS_COMMON_SOURCES
                $PHP_PERFIDIOUS_LINUX_SOURCES
            ])
            PHP_ADD_BUILD_DIR(src/linux)
        ],
        [darwin*], [
            AC_DEFINE(
                [PERFIDIOUS_PLATFORM_DARWIN],
                [1],
                [Define to 1 when building the Darwin backend]
            )
            PHP_PERFIDIOUS_ADD_SOURCES([
                $PHP_PERFIDIOUS_COMMON_SOURCES
                $PHP_PERFIDIOUS_DARWIN_SOURCES
            ])
            PHP_ADD_BUILD_DIR(src/darwin)
        ],
        [AC_MSG_ERROR([perfidious supports only Linux and Darwin on Unix-like systems])]
    )

    dnl AX_COMPILER_FLAGS defaults --enable-compile-warnings to "error" (fatal warnings) unless
    dnl ax_is_release=yes, and the ordinary [git-directory] policy sets ax_is_release=no for any
    dnl checkout with a .git directory - which would make -Werror the default for every plain
    dnl `git clone && phpize && ./configure`, not just for our own dev environment. Key it off
    dnl IN_NIX_SHELL instead: -Werror stays the default inside our nix devShell (where we want it
    dnl to catch warnings), but a plain git checkout gets the same lenient default as a PECL/
    dnl tarball install. This can always be overridden explicitly with --enable-compile-warnings.
    AS_IF([test -n "$IN_NIX_SHELL"],
          [AX_IS_RELEASE([never])],
          [AX_IS_RELEASE([always])])
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
        -Wno-nested-externs -Wno-error=nested-externs \
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

    if test "$PHP_PERFIDIOUS_COVERAGE" == "yes"; then
        CFLAGS="-fprofile-arcs -ftest-coverage $CFLAGS"
        LDFLAGS="--coverage $LDFLAGS"
    fi

    if test "$PHP_PERFIDIOUS_SANITIZE" == "yes"; then
        PERFIDIOUS_EXTRA_CFLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g"
    fi

    PHP_ADD_BUILD_DIR(src)
    PHP_INSTALL_HEADERS([ext/perfidious], [php_perfidious.h])
    dnl 4th arg (sapi_class) intentionally left blank: -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 used to
    dnl sit there, which PHP_NEW_EXTENSION only ever compares against the literal string "cli" -
    dnl it never did anything. extra-cflags is the 5th arg.
    PHP_NEW_EXTENSION(perfidious, $PHP_PERFIDIOUS_SOURCES, $ext_shared, , -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 $PERFIDIOUS_EXTRA_CFLAGS)
    PHP_ADD_EXTENSION_DEP(perfidious, spl, false)
    PHP_ADD_EXTENSION_DEP(perfidious, opcache, true)
    PHP_SUBST(PERFIDIOUS_SHARED_LIBADD)
fi
