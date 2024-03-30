{
  lib,
  php,
  stdenv,
  libpfm,
  pkg-config,
  autoreconfHook,
  buildPecl,
  src,
}:
buildPecl rec {
  pname = "perf";
  name = "perf-${version}";
  version = "v0.1.0";

  inherit src;

  buildInputs = [libpfm];

  passthru = {
    inherit php libpfm;
  };

  makeFlags = ["phpincludedir=$(dev)/include"];
  outputs = ["out" "dev"];

  #TEST_PHP_DETAILED = 1;
  NO_INTERACTION = 1;
  REPORT_EXIT_STATUS = 1;
  TEST_PHP_ARGS = "-c ${php.phpIni}";
}
