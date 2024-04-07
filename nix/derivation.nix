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
  debugSupport ? false,
  WerrorSupport ? checkSupport,
  valgrindSupport ? true,
}:
(buildPecl rec {
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
    ++ lib.optionals (!WerrorSupport) ["--enable-compile-warnings=yes" "--disable-Werror"]
    ++ lib.optional debugSupport "--enable-perfidious-debug";

  makeFlags = ["phpincludedir=$(dev)/include"];
  outputs = ["out" "dev"];

  doCheck = checkSupport;
  theRealFuckingCheckPhase =
    ''
      REPORT_EXIT_STATUS=1 NO_INTERACTION=1 make test TEST_PHP_ARGS="-n" || (find tests -name '*.log' | xargs cat ; exit 1)
    ''
    + (lib.optionalString valgrindSupport ''
      USE_ZEND_ALLOC=0 REPORT_EXIT_STATUS=1 NO_INTERACTION=1 make test TEST_PHP_ARGS="-n -m" || (find tests -name '*.mem' | xargs cat ; exit 1)
    '');

  #TEST_PHP_DETAILED = 1;
})
.overrideAttrs (o:
    o
    // {
      checkPhase = o.theRealFuckingCheckPhase;
    })
