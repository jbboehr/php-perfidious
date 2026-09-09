{
  pkgs,
  src,
}: let
  inherit (pkgs) lib;

  # PHP's official Windows development packs include configured headers and import libraries.
  # Hashes come from https://downloads.php.net/~windows/releases/releases.json.
  phps = {
    php81 = {
      version = "8.1.34";
      compiler = "vs16";
      nts = "d5981b1336b77de96461c0a7b5a54dda8fb7aed03df29a17c634f36219eeede4";
      zts = "65c01d508cac78f2f317a61f1ead0df14cf79a2233aad70311d44cf980fd8889";
    };
    php82 = {
      version = "8.2.33";
      compiler = "vs16";
      nts = "d4d0da6e6f1ad3f9e058262261fc43d7fa329b212207e4bfb2a39ad0b39ee891";
      zts = "46c00fc49b8cf6c35a4a8b5269114b1da90ef3a001f5ee1cd577f7c12ab6aad9";
    };
    php83 = {
      version = "8.3.33";
      compiler = "vs16";
      nts = "49fa1880cea4233b8c6128c84f01a1d5fc4de7e9c97854b1b832eef37f558a1c";
      zts = "d58cedfa76b74b49f88ee54d25426700a27dad76dbad4732440cf28ca8267a7c";
    };
    php84 = {
      version = "8.4.25";
      compiler = "vs17";
      nts = "55fd07f549c0494cfde2827dfe90ce128ff0359df7dbc76f7617ed1adcdefa48";
      zts = "75e4555ef32cb1524968f3019eb4567c06e89be13b1641148375fee99658b8b4";
    };
    php85 = {
      version = "8.5.10";
      compiler = "vs17";
      nts = "b277dafab9654b23dec28fdebe47385248fdd33bbe8ecdc33b8681ef1b5c7788";
      zts = "0031d279f13f21e81fd62f9a98e919f28b1875ba457916d60daed85586e479dd";
    };
  };

  manifest = pkgs.fetchurl {
    url = "https://download.visualstudio.microsoft.com/download/pr/f8d58e41-102a-4347-a567-60d7235da3b5/6b3c2fa48fca73bf296bf59ce63a12c9632380cab039882dc33e8a650534b1d3/VisualStudio.17.Release.chman";
    sha256 = "b2631afb766b66de6c75da91efe3cecc8a56c8ca5c59c0beaa6227d963189b0f";
  };
  sdk =
    pkgs.runCommand "perfidious-xwin-sdk" {
      nativeBuildInputs = [pkgs.xwin];
      SSL_CERT_FILE = "${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt";
      outputHashMode = "recursive";
      outputHashAlgo = "sha256";
      outputHash = "sha256-UFQjsFVBwcF/9e9tVFoG0Z1JySxyTnFqoaRwr/tUWzA=";
    } ''
      xwin --accept-license --manifest ${manifest} --arch x86_64 \
        splat --copy --output "$out"
    '';

  # Match the MSVC-built PHP ABI: vectorcall functions and no Clang-only allocator exports.
  phpAbi = pkgs.writeText "php-msvc-abi.h" ''
    #include "main/config.w32.h"
    #include "Zend/zend_portability.h"
    #undef ZEND_FASTCALL
    #define ZEND_FASTCALL __vectorcall
    #undef HAVE_BUILTIN_CONSTANT_P
  '';

  makePackage = {
    php,
    ts,
  }: let
    release = phps.${php};
    archive = "php-devel-pack-${release.version}${lib.optionalString (ts == "nts") "-nts"}-Win32-${release.compiler}-x64.zip";
    devel = pkgs.fetchurl {
      urls = map (directory: "https://downloads.php.net/~windows/releases/${directory}${archive}") ["" "archives/"];
      sha256 = release.${ts};
    };
  in
    pkgs.runCommand "perfidious-${php}-windows-${ts}" {
      nativeBuildInputs = [pkgs.unzip pkgs.llvmPackages.clang-unwrapped pkgs.llvmPackages.lld pkgs.llvmPackages.llvm];
    } ''
      unzip -q ${devel}
      phpdev="$PWD/php-${release.version}-devel-${release.compiler}-x64"
      mkdir include
      # PHP uses a casing that xwin's default aliases do not cover.
      ln -s ${sdk}/sdk/include/um/ws2tcpip.h include/Ws2tcpip.h

      clang-cl --target=x86_64-pc-windows-msvc -fuse-ld=lld /MD /O2 /LD /std:c11 \
        /DPHP_WIN32 /DZEND_WIN32 /DWIN32 /D_WINDOWS /DNDEBUG /DZEND_DEBUG=0 \
        ${lib.optionalString (ts == "zts") "/DZTS=1"} \
        /DCOMPILE_DL_PERFIDIOUS /DPERFIDIOUS_PLATFORM_WINDOWS=1 /DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 \
        /FI${phpAbi} /I${src} /Iinclude \
        -imsvc "$phpdev/include" -imsvc "$phpdev/include/main" \
        -imsvc "$phpdev/include/Zend" -imsvc "$phpdev/include/TSRM" \
        -imsvc ${sdk}/crt/include -imsvc ${sdk}/sdk/include/ucrt \
        -imsvc ${sdk}/sdk/include/um -imsvc ${sdk}/sdk/include/shared \
        ${src}/src/extension.c ${src}/src/exceptions.c ${src}/src/read_result.c \
        ${src}/src/sampler.c ${src}/src/platform.c \
        ${src}/src/windows/functions.c ${src}/src/windows/sampler.c ${src}/src/windows/thread_profile.c \
        /Fe:php_perfidious.dll /link /libpath:"$phpdev/lib" \
        /libpath:${sdk}/crt/lib/x86_64 /libpath:${sdk}/sdk/lib/ucrt/x86_64 \
        /libpath:${sdk}/sdk/lib/um/x86_64 php8${lib.optionalString (ts == "zts") "ts"}.lib kernel32.lib psapi.lib

      llvm-readobj --file-headers --coff-exports --coff-imports php_perfidious.dll > headers.txt
      grep -F 'IMAGE_FILE_MACHINE_AMD64' headers.txt
      grep -F 'IMAGE_FILE_DLL' headers.txt
      grep -Fx '  Name: get_module' headers.txt
      grep -Fx '  Name: php8${lib.optionalString (ts == "zts") "ts"}.dll' headers.txt
      mkdir -p "$out/lib/php/extensions"
      cp php_perfidious.dll "$out/lib/php/extensions/"
    '';
in
  builtins.listToAttrs (map (config:
      lib.nameValuePair "windows-${config.php}-${config.ts}" (makePackage config))
    (lib.cartesianProduct {
      php = builtins.attrNames phps;
      ts = ["nts" "zts"];
    }))
