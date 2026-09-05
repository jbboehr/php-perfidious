# Copyright (c) anno Domini nostri Jesu Christi MMXVI-MMXXIV John Boehr & contributors
#
# SPDX-License-Identifier: AGPL-3.0-only WITH romic-exception
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License version 3,
# as published by the Free Software Foundation, together with the Romic
# Exception (an additional permission under section 7 of that license).
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# and the Romic Exception along with this program.  If not, see
# <http://www.gnu.org/licenses/> and the LICENSE_EXCEPTION file.
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
              nix/checks/
              nix/vm-test/
              tests/valgrind/
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

        makeCapabilityLeakCheck = package:
          pkgs.runCommand "perfidious-capability-leak-check" {
            nativeBuildInputs = [
              pkgs.stdenv.cc
              pkgs.valgrind
            ];
          } ''
            export USE_ZEND_ALLOC=0
            valgrind \
              --quiet \
              --keep-debuginfo=yes \
              --leak-check=full \
              --show-leak-kinds=definite \
              --errors-for-leak-kinds=definite \
              --error-exitcode=1 \
              --suppressions=${./tests/valgrind/libpfm-initialize.supp} \
              ${package.php.unwrapped}/bin/php \
              -n \
              -d extension=${package}/lib/php/extensions/perfidious.so \
              ${./nix/checks/capability-leak.php}

            cc \
              -shared \
              -fPIC \
              -g \
              -O1 \
              -fno-omit-frame-pointer \
              -fno-optimize-sibling-calls \
              -Wall \
              -Wextra \
              -Werror \
              $(${package.php.unwrapped.dev}/bin/php-config --includes) \
              ${./tests/valgrind/libpfm-suppression-scope.c} \
              -o libpfm-suppression-scope.so

            if valgrind \
              --quiet \
              --keep-debuginfo=yes \
              --leak-check=full \
              --show-leak-kinds=definite \
              --errors-for-leak-kinds=definite \
              --error-exitcode=41 \
              --suppressions=${./tests/valgrind/libpfm-initialize.supp} \
              ${package.php.unwrapped}/bin/php \
              -n \
              -d extension=./libpfm-suppression-scope.so \
              -r ""
            then
              echo "libpfm suppression hid the unrelated MINIT leak" >&2
              exit 1
            else
              status=$?
            fi

            if [ "$status" -ne 41 ]; then
              echo "Valgrind returned unexpected status $status for the suppression-scope fixture" >&2
              exit 1
            fi

            touch $out
          '';

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
            # LICENSE.md: verbatim upstream GPL/AGPL boilerplate, not worth reformatting.
            # PULL_REQUEST_TEMPLATE.md: GitHub PR templates conventionally start with ## (no H1)
            # and use <details>/<summary> for collapsible sections.
            markdownlint.excludes = ["LICENSE\.md" "\.github/PULL_REQUEST_TEMPLATE\.md"];
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

        makeVmCheck = {
          package,
          runPhpt ? true,
        }: let
          php = package.php.buildEnv {
            extensions = {
              enabled,
              all,
            }:
            # not all php versions have opcache packaged as a selectable extension yet
            # (e.g. php85, as of this nixpkgs revision)
              enabled ++ lib.optional (all ? opcache) all.opcache ++ [package];
          };

          # CLI tests see one request per process. Serve this docroot over php-fpm to exercise
          # the request handle's reuse across requests and its repeated reset/enable/disable lifecycle.
          #
          # note: this deliberately does NOT assert on real counter magnitudes (e.g. timeEnabled
          # growing across requests) - perf counters were found to be unreliable/frozen inside
          # nested-virtualization CI runners (confirmed even for a single non-forked long-lived
          # process), which is also why the CLI .phpt suite never asserts real counter values.
          # What's actually novel/valuable here - and fully deterministic regardless of PMU/timer
          # virtualization quirks - is proving the *same* worker process survives many repeated
          # RINIT/RSHUTDOWN cycles (i.e. real requests) without request_handle()
          # erroring, returning corrupt data, or crashing the worker: exactly the class of
          # use-after-free / double-reset bug a one-shot-per-process CLI test can never catch.
          fpmDocroot = ./nix/vm-test;
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

                services.nginx = {
                  enable = true;
                  virtualHosts."perfidious" = {
                    root = "${fpmDocroot}";
                    locations = {
                      "~ \\.php$".extraConfig = ''
                        fastcgi_pass unix:${config.services.phpfpm.pools.perfidious.socket};
                        fastcgi_index index.php;
                        include ${config.services.nginx.package}/conf/fastcgi_params;
                        include ${pkgs.nginx}/conf/fastcgi.conf;
                      '';
                      "/".extraConfig = ''
                        try_files $uri $uri/ index.php;
                      '';
                    };
                  };
                };

                services.phpfpm.pools.perfidious = {
                  user = "nginx";
                  phpPackage = php;
                  # pinned to exactly one static worker: every request must land on the same
                  # process so the request handle's repeated reset/enable/disable lifecycle is exercised
                  # software events, not the default perf::PERF_COUNT_HW_* metrics: nested-virtualized
                  # CI runners (e.g. GitHub Actions) commonly don't expose a hardware PMU to the guest
                  # at all, so perf_event_open() for a HW event fails outright there ("No such file or
                  # directory") - software events don't depend on hardware PMU virtualization support
                  phpOptions = ''
                    perfidious.request.enable = 1
                    perfidious.request.metrics = "perf::PERF_COUNT_SW_TASK_CLOCK:u"
                  '';
                  settings = {
                    "listen.owner" = "nginx";
                    "listen.group" = "nginx";
                    "listen.mode" = "0600";
                    "pm" = "static";
                    "pm.max_children" = 1;
                  };
                };
              };
            };
            testScript = {nodes, ...}: ''
              import json

              def request(query=""):
                  out = machine1.succeed(f"curl -fsS 'http://127.0.0.1:80/index.php{query}'")
                  return json.loads(out)

              machine1.wait_for_unit("default.target")
              machine1.succeed("php -m && php -m | grep -i perfidious")
              ${lib.optionalString runPhpt ''
                machine1.succeed("cp -r --no-preserve=mode,ownership ${src}/* .")
                machine1.succeed("cp --no-preserve=mode,ownership ${php.unwrapped.dev}/lib/build/run-tests.php .")
                machine1.succeed("TEST_PHP_DETAILED=1 NO_INTERACTION=1 REPORT_EXIT_STATUS=1 php run-tests.php || (find tests -name '*.log' | xargs -n1 cat ; exit 1)")
              ''}

              machine1.wait_for_unit("nginx.service")
              machine1.wait_for_unit("phpfpm-perfidious.service")

              readings = [request() for _ in range(10)]

              for i, r in enumerate(readings):
                  assert "error" not in r, f"request #{i}: request_handle() broken under php-fpm: {r}"

              pids = {r["pid"] for r in readings}
              assert len(pids) == 1, (
                  f"expected all {len(readings)} requests to land on the same persistent php-fpm "
                  f"worker (pm=static, pm.max_children=1), got worker pids: {sorted(pids)}"
              )

              ${lib.optionalString package.debugSupport ''
                injected = request("?failRequestHandleShutdown=1")
                assert injected["requestHandleShutdownFailureInjected"] is True

                recovered_error = request()
                assert recovered_error["pid"] == injected["pid"], (
                    f"transient request-handle lifecycle failure killed worker {injected['pid']}; "
                    f"replacement worker is {recovered_error['pid']}"
                )
                assert recovered_error["requestHandleError"] != 0, recovered_error

                recovered = request()
                assert recovered["pid"] == injected["pid"], recovered
                assert "error" not in recovered and "requestHandleError" not in recovered, recovered

                broken = request("?breakRequestHandle=1")
                assert broken["requestHandleBroken"] is True

                unavailable = request()
                assert unavailable["pid"] == broken["pid"], (
                    f"request-handle lifecycle failure killed worker {broken['pid']}; "
                    f"replacement worker is {unavailable['pid']}"
                )
                assert unavailable["requestHandleError"] != 0, unavailable
              ''}
            '';
          };

        pkgs-phps = nix-phps.packages.${system};

        php85Zts =
          (pkgs.php85.override {
            argon2Support = false;
            cgiSupport = false;
            fpmSupport = false;
            pearSupport = false;
            pharSupport = false;
            phpdbgSupport = false;
            systemdSupport = false;
            ztsSupport = true;
          }).buildEnv {
            extensions = _: [];
          };

        php85ZtsCheck = (makeCheck (makePackage {php = php85Zts;})).overrideAttrs (previous: {
          preCheck =
            (previous.preCheck or "")
            + ''
              ${php85Zts}/bin/php -r 'exit(PHP_ZTS ? 0 : 1);'
            '';
        });

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
            coverageSupport = [false];
          })
          ++ [
            {
              php = "php81";
              stdenv = "gcc";
              debugSupport = true;
            }
          ]
          ++ (lib.cartesianProduct {
            php = ["php81" "php82" "php83" "php84" "php85"];
            stdenv = ["gcc"];
            debugSupport = [false true];
            coverageSupport = [true];
          });

        buildFn = {
          php,
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
            nativeBuildInputs = [sanitizeStdenv.cc pkgs.php82 sanitizeStaticPhp.dev];
          } ''
            cp -r --no-preserve=mode,ownership ${src}/tests .
            cp -r --no-preserve=mode,ownership ${src}/stubs .
            cp -r --no-preserve=mode,ownership ${src}/src .
            cp --no-preserve=mode,ownership ${src}/php_perfidious.h .
            cp ${sanitizeStaticPhp.dev}/lib/build/run-tests.php .

            export USE_ZEND_ALLOC=0
            export LD_PRELOAD="$(cc -print-file-name=libasan.so):$(cc -print-file-name=libubsan.so)"
            export ASAN_OPTIONS="detect_leaks=0:abort_on_error=1"
            export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"
            export NO_INTERACTION=1
            export REPORT_EXIT_STATUS=1
            export PERFIDIOUS_STUB_PHP=${pkgs.php82}/bin/php

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
          {
            inherit pre-commit-check;
            php81-gcc-capability-leak = makeCapabilityLeakCheck packages.php81-gcc;
            php85-zts = php85ZtsCheck;
            php81-gcc-vmtest = makeVmCheck {package = packages.php81-gcc;};
            php81-gcc-debug-vmtest = makeVmCheck {
              package = packages.php81-gcc-debug;
              runPhpt = false;
            };
            php85-gcc-vmtest = makeVmCheck {package = packages.php85-gcc;};
          }
          // (builtins.mapAttrs (name: package: makeCheck package) (builtins.removeAttrs packages ["default"]));

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
