{
  pkgs,
  buildPkgs,
  php,
  src,
  projectSource,
  libc,
}: let
  inherit (buildPkgs) lib;
  releasePhp =
    if libc == "glibc"
    then php
    else
      ((php.override {
        argon2Support = false;
        cgiSupport = false;
        fpmSupport = false;
        pearSupport = false;
        pharSupport = false;
        phpdbgSupport = false;
        systemdSupport = false;
        valgrindSupport = false;
        ztsSupport = false;
      }).buildEnv {extensions = _: [];})
      .overrideAttrs (_: {
        # x64 musl executables run on the x64 build host. Run PHP's feature probes;
        # its cross-compilation defaults incorrectly select off64_t on PHP 8.1.
        configurePlatforms = [];
      });

  staticCap =
    (pkgs.libcap.override {
      isStatic = true;
      usePam = false;
      withGo = false;
    }).overrideAttrs (previous: {
      makeFlags = previous.makeFlags ++ ["-C" "libcap"];
      outputs = ["out" "dev" "lib" "doc"];
      env = (previous.env or {}) // {NIX_CFLAGS_COMPILE = "-std=gnu11";};
    });
  staticPfm = (pkgs.libpfm.override {enableShared = false;}).overrideAttrs (previous: {
    preBuild = ''makeFlagsArray+=("DIRS=lib include")'';
    env = previous.env // {NIX_CFLAGS_COMPILE = "-fPIC -std=gnu11 -Wno-error";};
  });
  module =
    (buildPkgs.callPackage ./derivation.nix {
      inherit src;
      inherit (pkgs) stdenv;
      php = releasePhp;
      libcap = staticCap;
      libpfm = staticPfm;
      buildPecl = releasePhp.buildPecl;
      checkSupport = true;
      valgrindSupport = false;
    }).overrideAttrs (previous: {
      env = (previous.env or {}) // {NIX_CFLAGS_COMPILE = "-std=gnu11";};
      preBuild = ''
        makeFlagsArray+=("PERFIDIOUS_SHARED_LIBADD=${staticCap.lib}/lib/libcap.a ${staticPfm}/lib/libpfm.a -Wl,--exclude-libs,ALL")
      '';
    });
in
  buildPkgs.runCommand "perfidious-${module.version}-php${lib.versions.majorMinor releasePhp.version}-${libc}-zip" {
    nativeBuildInputs = [buildPkgs.python3 buildPkgs.binutils buildPkgs.patchelf releasePhp];
    passthru = {inherit module;};
  } ''
    mkdir -p modules docs dependencies
    cp ${module}/lib/php/extensions/perfidious.so modules/
    chmod u+w modules/perfidious.so
    # Clear the old string as well as its ELF tag, so no store paths remain in the ZIP.
    patchelf --set-rpath "" modules/perfidious.so
    patchelf --remove-rpath modules/perfidious.so
    ${lib.optionalString (libc == "musl") ''
      patchelf --replace-needed libc.so libc.musl-x86_64.so.1 modules/perfidious.so
    ''}
    cp ${projectSource}/LICENSE.md .
    cp ${projectSource}/docs/LICENSE_EXCEPTION.md docs/
    cp ${staticCap.src} dependencies/libcap.tar.xz
    cp ${staticPfm.src} dependencies/libpfm.tar.gz
    cat > dependencies/README.txt <<'EOF'
    This module statically links libcap ${staticCap.version} and libpfm ${staticPfm.version}.
    Their complete upstream sources, including copyright notices and license terms,
    accompany the binary in libcap.tar.xz (License) and libpfm.tar.gz (COPYING).
    The release build uses libcap under its BSD license and libpfm under its MIT license.
    See nix/release-linux.nix in the tagged perfidious source for the build recipe.
    EOF
    php -n -d extension=${module}/lib/php/extensions/perfidious.so -r \
      'if (phpversion("perfidious") !== "${lib.removePrefix "v" module.version}") { throw new RuntimeException("Release version mismatch"); }'
    python3 ${projectSource}/tools/package-linux.py ${module.version} ${libc} "$out"
  ''
