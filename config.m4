
PHP_ARG_ENABLE(perf, whether to enable perf support,
[  --enable-perf     Enable perf support])

AC_DEFUN([PHP_PERF_ADD_SOURCES], [
  PHP_PERF_SOURCES="$PHP_PERF_SOURCES $1"
])

if test "$PHP_PERF" != "no"; then
    PHP_PERF_ADD_SOURCES([
        src/extension.c
    ])

    PHP_ADD_BUILD_DIR(src)
    PHP_INSTALL_HEADERS([ext/perf], [php_perf.h])
    PHP_NEW_EXTENSION(perf, $PHP_PERF_SOURCES, $ext_shared, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
    PHP_SUBST(PERF_SHARED_LIBADD)
fi
