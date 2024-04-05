{
  lib,
  php,
  stdenv,
  libcap,
  libpfm,
  pkg-config,
  valgrind,
  autoreconfHook,
  buildPecl,
  src,
  checkSupport ? false,
  WerrorSupport ? checkSupport,
  valgrindSupport ? true,
}:
buildPecl rec {
  pname = "perfidious";
  name = "perfidious-${version}";
  version = "v0.1.0";

  inherit src;

  buildInputs = [libcap libpfm];
  nativeBuildInputs =
    [php.unwrapped.dev pkg-config]
    ++ lib.optional valgrindSupport valgrind;

  passthru = {
    inherit php libpfm stdenv;
  };

  configureFlags =
    []
    ++ lib.optional WerrorSupport "--enable-compile-warnings=error"
    ++ lib.optionals (!WerrorSupport) ["--enable-compile-warnings=yes" "--disable-Werror"];

  makeFlags = ["phpincludedir=$(dev)/include"];
  outputs = ["out" "dev"];

  doCheck = checkSupport;
  checkPhase =
    ''
      NO_INTERACTON=yes REPORT_EXIT_STATUS=yes make test
    ''
    + (lib.optionalString valgrindSupport ''
      USE_ZEND_ALLOC=0 NO_INTERACTON=yes REPORT_EXIT_STATUS=yes make test TEST_PHP_ARGS=-m
    '');

  #TEST_PHP_DETAILED = 1;
}
