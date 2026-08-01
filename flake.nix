# Copyright (c) anno Domini nostri Jesu Christi MMXVI-MMXXIV John Boehr & contributors
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
{
  description = "php-perfidious";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-26.05";
    nixpkgs-unstable.url = "github:nixos/nixpkgs/nixos-unstable";
    systems.url = "github:nix-systems/default-linux";
    flake-utils = {
      url = "github:numtide/flake-utils";
      inputs.systems.follows = "systems";
    };
    gitignore = {
      url = "github:hercules-ci/gitignore.nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    pre-commit-hooks = {
      url = "github:cachix/pre-commit-hooks.nix";
      inputs.nixpkgs.follows = "nixpkgs";
      inputs.nixpkgs-stable.follows = "nixpkgs";
      inputs.gitignore.follows = "gitignore";
    };
    nix-github-actions = {
      url = "github:nix-community/nix-github-actions";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    nix-phps = {
      url = "github:fossar/nix-phps";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = {
    self,
    nixpkgs,
    nixpkgs-unstable,
    systems,
    flake-utils,
    gitignore,
    pre-commit-hooks,
    nix-github-actions,
    nix-phps,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
        pkgs-unstable = nixpkgs-unstable.legacyPackages.${system};
        lib = pkgs.lib;

        src' = gitignore.lib.gitignoreSource ./.;

        iwyu = pkgs.callPackage ./nix/iwyu.nix {};

        src = pkgs.lib.cleanSourceWith {
          name = "php-perfidious-source";
          src = src';
          filter = gitignore.lib.gitignoreFilterWith {
            basePath = ./.;
            extraRules = ''
              .clang-format
              composer.json
              composer.lock
              .editorconfig
              .envrc
              .gitattributes
              .github
              .gitignore
              *.md
              *.nix
              flake.*
            '';
          };
        };

        makePackage = {
          stdenv ? pkgs.stdenv,
          php ? pkgs.php,
          libpfm ? pkgs.libpfm,
          debugSupport ? false,
          coverageSupport ? false,
        }:
          pkgs.callPackage ./nix/derivation.nix {
            inherit src;
            inherit stdenv php libpfm;
            inherit debugSupport coverageSupport;
            buildPecl = pkgs.callPackage (nixpkgs + "/pkgs/build-support/php/build-pecl.nix") {
              inherit php stdenv;
            };
          };

        makeCheck = package:
          package.override {
            checkSupport = true;
            WerrorSupport = true;
          };

        pre-commit-check = pre-commit-hooks.lib.${system}.run {
          src = src';
          hooks = {
            actionlint.enable = true;
            alejandra.enable = true;
            alejandra.excludes = ["\/vendor\/"];
            # I hate formatters
            #clang-format.enable = true;
            #clang-format.types_or = ["c" "c++"];
            #clang-format.files = "\\.(c|h)$";
            markdownlint.enable = true;
            markdownlint.excludes = ["LICENSE\.md"];
            markdownlint.settings.configuration = {
              MD013 = {
                line_length = 1488;
                # this doesn't seem to work
                table = false;
              };
              # Keep a Changelog's format repeats ### Added / ### Fixed / etc under every
              # version heading by design - only flag duplicates within the same parent section
              MD024 = {
                siblings_only = true;
              };
            };
            shellcheck.enable = true;
          };
        };

        makeDevShell = package:
          (pkgs.mkShell.override {
            stdenv = package.stdenv;
          }) {
            inputsFrom = [package];
            buildInputs = with pkgs; [
              actionlint
              autoconf-archive
              clang-tools
              iwyu
              lcov
              perf
              gdb
              gh
              package.php.packages.composer
              valgrind
            ];
            shellHook = ''
              ${pre-commit-check.shellHook}
              mkdir -p .direnv/include
              unlink .direnv/include/php
              ln -sf ${package.php.unwrapped.dev}/include/php/ .direnv/include/php
              unlink .direnv/include/perfmon
              ln -sf ${package.libpfm}/include/perfmon .direnv/include/perfmon
              export REPORT_EXIT_STATUS=1
              export NO_INTERACTION=1
              export PATH="$PWD/vendor/bin:$PATH"
              # opcache isn't getting loaded for tests because tests are run with '-n' and nixos doesn't compile
              # in opcache and relies on mkWrapper to load extensions
              export TEST_PHP_ARGS='-c ${package.php.phpIni}'
              # php.unwrapped from the buildDeps is overwriting php
              export PATH="${package.php}/bin:./vendor/bin:$PATH"
            '';
          };

        makeVmCheck = package: let
          php = package.php.buildEnv {
            extensions = {
              enabled,
              all,
            }:
            # not all php versions have opcache packaged as a selectable extension yet
            # (e.g. php85, as of this nixpkgs revision)
              enabled ++ lib.optional (all ? opcache) all.opcache ++ [package];
          };
        in
          pkgs.testers.runNixOSTest {
            name = "php-perfidious-vm-test";
            qemu.package = pkgs.qemu_full;
            nodes = {
              machine1 = {
                config,
                pkgs,
                ...
              }: {
                virtualisation.qemu.options = ["-cpu host"];
                boot.kernel.sysctl."kernel.perf_event_paranoid" = -1;
                boot.kernel.sysctl."kernel.kptr_restrict" = lib.mkForce 0;
                environment.systemPackages = [
                  php
                ];
              };
            };
            testScript = {nodes, ...}: ''
              machine.wait_for_unit("default.target")
              machine.succeed("php -m && php -m | grep -i perfidious")
              machine.succeed("cp -r --no-preserve=mode,ownership ${src}/* .")
              machine.succeed("cp --no-preserve=mode,ownership ${php.unwrapped.dev}/lib/build/run-tests.php .")
              machine.succeed("TEST_PHP_DETAILED=1 NO_INTERACTION=1 REPORT_EXIT_STATUS=1 php run-tests.php || (find tests -name '*.log' | xargs -n1 cat ; exit 1)")
            '';
          };

        pkgs-phps = nix-phps.packages.${system};

        matrix = with pkgs; {
          php = {
            inherit php82 php83 php85;
            php81 = pkgs-phps.php81;
            php84 = pkgs-unstable.php84;
          };
          stdenv = {
            gcc = stdenv;
            clang = clangStdenv;
            musl = pkgsMusl.stdenv;
          };
          libpfm = {
            inherit libpfm;
          };
        };

        # @see https://github.com/NixOS/nixpkgs/pull/110787
        buildConfs =
          (lib.cartesianProduct {
            php = ["php81" "php82" "php83" "php84" "php85"];
            stdenv = [
              "gcc"
              "clang"
              # totally broken
              # "musl"
            ];
            libpfm = ["libpfm"];
            coverageSupport = [false];
          })
          ++ [
            {
              php = "php81";
              stdenv = "gcc";
              libpfm = "libpfm";
              debugSupport = true;
            }
          ]
          ++ (lib.cartesianProduct {
            php = ["php81" "php82" "php83" "php84" "php85"];
            stdenv = ["gcc"];
            libpfm = ["libpfm"];
            debugSupport = [false true];
            coverageSupport = [true];
          });

        buildFn = {
          php,
          libpfm,
          stdenv,
          debugSupport ? false,
          coverageSupport ? false,
        }:
          lib.nameValuePair
          (lib.concatStringsSep "-" (lib.filter (v: v != "") [
            "${php}"
            "${stdenv}"
            #(if stdenv == "gcc" then "" else "${stdenv}")
            (
              if libpfm == "libpfm"
              then ""
              else "${libpfm}"
            )
            (
              if debugSupport
              then "debug"
              else ""
            )
            (
              if coverageSupport
              then "coverage"
              else ""
            )
          ]))
          (
            makePackage {
              php = matrix.php.${php};
              libpfm = matrix.libpfm.${libpfm};
              stdenv = matrix.stdenv.${stdenv};
              inherit debugSupport coverageSupport;
            }
          );

        packages' = builtins.listToAttrs (builtins.map buildFn buildConfs);
        packages =
          packages'
          // {
            # php81 = packages.php81-gcc;
            # php82 = packages.php82-gcc;
            # php83 = packages.php83-gcc;
            # php84 = packages.php84-gcc;
            default = packages.php81-gcc;
          };

        # Statically-linked, whole-process ASan/UBSan build: perfidious source is embedded
        # directly into a php source tree (as ext/perfidious) and built in, so there's no
        # dlopen() of a separate .so at all. That matters because PHP's extension loader always
        # dlopen()s with RTLD_DEEPBIND, which is fundamentally incompatible with sanitizer
        # runtimes - an ordinary dynamically-loaded .so built with -fsanitize=address just can't
        # be loaded at all, so a ASan/UBSan build has to avoid dlopen() entirely like this.
        #
        # Deliberately NOT wired into `packages`/`checks`/`devShells`: it rebuilds the whole of
        # PHP core from source (slow), so it's opt-in only via
        # `nix build .#sanitize-static-php82` / `.#sanitize-static-php82-check`.
        sanitizeStdenv =
          if pkgs.stdenv.cc.isClang
          then pkgs.llvmPackages.stdenv
          else pkgs.stdenv;

        sanitizeStaticPhp = let
          basePhp = pkgs.php82.unwrapped;
        in
          pkgs.callPackage (nixpkgs + "/pkgs/development/interpreters/php/generic.nix") {
            stdenv = sanitizeStdenv;
            pcre2 = pkgs.pcre2.override {withJitSealloc = false;};
            version = basePhp.version;
            phpSrc = basePhp.src;
            phpAttrsOverrides = final: prev: {
              buildInputs = prev.buildInputs ++ [pkgs.libcap pkgs.libpfm];
              postPatch =
                (prev.postPatch or "")
                + ''
                  cp -r ${src} ext/perfidious
                  chmod -R u+w ext/perfidious
                  # config.m4's m4_include(m4/...) paths resolve relative to php-src's own root
                  # (which has no m4/ dir of its own), not relative to ext/perfidious/
                  cp -r ext/perfidious/m4 m4
                '';
              configureFlags =
                prev.configureFlags
                ++ [
                  "--enable-perfidious"
                  "--enable-perfidious-sanitize"
                  "--enable-compile-warnings=yes"
                  "--disable-Werror"
                ];
              # only LDFLAGS (needed on the final link, to pull in the sanitizer runtime) applies
              # to the whole build - CFLAGS stays scoped to just perfidious's own object files,
              # via --enable-perfidious-sanitize above, see config.m4
              LDFLAGS = "${prev.LDFLAGS or ""} -fsanitize=address,undefined";
            };
          };

        sanitizeStaticPhpCheck =
          pkgs.runCommand "perfidious-sanitize-static-check" {
            nativeBuildInputs = [sanitizeStdenv.cc];
          } ''
            cp -r --no-preserve=mode,ownership ${src}/tests .
            cp ${sanitizeStaticPhp.dev}/lib/build/run-tests.php .

            export USE_ZEND_ALLOC=0
            export LD_PRELOAD="$(cc -print-file-name=libasan.so):$(cc -print-file-name=libubsan.so)"
            export ASAN_OPTIONS="detect_leaks=0:abort_on_error=1"
            export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"
            export NO_INTERACTION=1
            export REPORT_EXIT_STATUS=1

            ${sanitizeStaticPhp}/bin/php -n run-tests.php \
              || (find tests -name '*.log' | xargs -r cat; exit 1)

            touch $out
          '';
      in {
        # sanitize-static-php82(-check) are intentionally added only here, to the *returned*
        # packages set, not the `packages` variable devShells/checks below are built from - see
        # sanitizeStaticPhp's comment above.
        packages =
          packages
          // {
            sanitize-static-php82 = sanitizeStaticPhp;
            sanitize-static-php82-check = sanitizeStaticPhpCheck;
          };

        devShells = builtins.mapAttrs (name: package: makeDevShell package) packages;

        checks =
          {inherit pre-commit-check;}
          // (builtins.mapAttrs (name: package: makeCheck package) packages)
          // (lib.mapAttrs' (name: value: lib.nameValuePair (name + "-vmtest") (makeVmCheck value)) packages);

        formatter = pkgs.alejandra;
      }
    )
    // {
      # prolly gonna break at some point
      githubActions.matrix.include = let
        cleanFn = v: v // {name = builtins.replaceStrings ["githubActions." "checks." "x86_64-linux."] ["" "" ""] v.attr;};
      in
        builtins.map cleanFn
        (nix-github-actions.lib.mkGithubMatrix {
          attrPrefix = "checks";
          checks = nixpkgs.lib.getAttrs ["x86_64-linux"] self.checks;
        })
        .matrix
        .include;
    };
}
