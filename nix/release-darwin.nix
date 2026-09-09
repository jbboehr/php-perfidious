{
  pkgs,
  php,
  src,
  projectSource,
  ztsSupport,
}: let
  inherit (pkgs) lib;
  releasePhp = (php.override {inherit ztsSupport;}).buildEnv {extensions = _: [];};
  module =
    (pkgs.callPackage ./derivation.nix {
      inherit src;
      php = releasePhp;
      buildPecl = releasePhp.buildPecl;
      libcap = null;
      libpfm = null;
      checkSupport = true;
      valgrindSupport = false;
    }).overrideAttrs (previous: {
      # Set this after setup hooks: the Nix build tools themselves can require newer macOS.
      preConfigure =
        (previous.preConfigure or "")
        + ''
          export MACOSX_DEPLOYMENT_TARGET=11.0
        '';
    });
in
  pkgs.runCommand "perfidious-${module.version}-php${lib.versions.majorMinor releasePhp.version}-darwin-${
    if ztsSupport
    then "zts"
    else "nts"
  }-zip" {
    nativeBuildInputs = [pkgs.bash pkgs.zip pkgs.python3 pkgs.darwin.cctools releasePhp];
    passthru = {inherit module;};
  } ''
    python3 ${projectSource}/nix/package-darwin.py
    mkdir -p modules docs
    cp ${module}/lib/php/extensions/perfidious.so modules/
    chmod u+w modules/perfidious.so
    while IFS= read -r rpath; do
      install_name_tool -delete_rpath "$rpath" modules/perfidious.so
    done < <(otool -l modules/perfidious.so | awk '
      $1 == "cmd" { rpath = ($2 == "LC_RPATH") }
      rpath && $1 == "path" { print $2 }
    ')
    # Editing Mach-O load commands invalidates its existing ad-hoc signature.
    source ${pkgs.darwin.signingUtils}
    sign modules/perfidious.so
    php -n -d extension="$PWD/modules/perfidious.so" -r '
      if (PHP_OS_FAMILY !== "Darwin" || PHP_INT_SIZE !== 8 || PHP_DEBUG
          || (bool) PHP_ZTS !== ${builtins.toJSON ztsSupport}
          || phpversion("perfidious") !== "${lib.removePrefix "v" module.version}") {
        throw new RuntimeException("Release version or PHP build mismatch");
      }
    '
    cp ${projectSource}/LICENSE.md .
    cp ${projectSource}/docs/LICENSE_EXCEPTION.md docs/
    touch -t 198001010000 modules/perfidious.so LICENSE.md docs/LICENSE_EXCEPTION.md
    bash ${projectSource}/nix/package-darwin.sh ${module.version} "$out"
  ''
