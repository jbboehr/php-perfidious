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
    agent-badge = {
      url = "github:jbboehr/agent-badge.ts/master";
      inputs.nixpkgs.follows = "nixpkgs";
    };
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
    agent-badge,
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

        # Keep the parser version aligned with the pinned PHP gen_stub.php.
        arginfoParser = pkgs.fetchFromGitHub {
          owner = "nikic";
          repo = "PHP-Parser";
          tag = "v5.6.1";
          hash = "sha256-h65fcmWoYLoOSzH5xQRTKe4FN5VYrhZy0c9tyG+EgfM=";
        };
        arginfoGenerator = pkgs.runCommand "perfidious-gen-stub" {} ''
          mkdir -p $out
          cp ${pkgs.php85.unwrapped.dev}/lib/build/gen_stub.php $out/gen_stub.php
          ln -s ${arginfoParser} $out/PHP-Parser-5.6.1
        '';
        generateArginfo = pkgs.writeShellApplication {
          name = "perfidious-generate-arginfo";
          runtimeInputs = [pkgs.bash pkgs.coreutils pkgs.diffutils pkgs.php85];
          text = ''
            export PERFIDIOUS_GEN_STUB=${arginfoGenerator}/gen_stub.php
            exec bash tools/generate-arginfo.sh "$@"
          '';
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
            arginfo = {
              enable = true;
              name = "Generated arginfo";
              entry = "${generateArginfo}/bin/perfidious-generate-arginfo --check";
              files = "^(stubs/.*\\.stub\\.php|src/.*_arginfo\\.h|tools/generate-arginfo\\.sh|flake\\.(nix|lock))$";
              pass_filenames = false;
            };
          };
        };

        makeDevShell = package:
          (pkgs.mkShell.override {
            stdenv = package.stdenv;
          }) {
            inputsFrom = [package];
            buildInputs = with pkgs; [
              actionlint
              agent-badge.packages.${system}.default
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
            # PHP 8.5 has built-in OpCache; earlier versions use a shared extension.
              enabled ++ lib.optional (all ? opcache) all.opcache ++ [package];
          };

          # Exercise request-handle reuse and reset/enable/disable across requests in one FPM worker.
          # This VM check avoids counter-magnitude assertions because nested virtualization can
          # prevent counters from advancing, even in a single process. Other PHPTs test live counts.
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
                environment.systemPackages =
                  [php]
                  ++ lib.optionals runPhpt [
                    pkgs.stdenv.cc
                    php.unwrapped.dev
                    pkgs.python3
                  ];
                users = lib.optionalAttrs runPhpt {
                  groups.phpt = {};
                  users.phpt = {
                    isSystemUser = true;
                    group = "phpt";
                  };
                };

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
                phpt_output = machine1.succeed("TEST_PHP_DETAILED=1 NO_INTERACTION=1 REPORT_EXIT_STATUS=1 php run-tests.php || (find tests -name '*.log' | xargs -n1 cat ; exit 1)")
                (driver.out_dir / "phpt.log").write_text(phpt_output, encoding="utf-8")

                # Non-root preloading runs in the FPM master and exercises inherited counters.
                machine1.succeed("install -d -o phpt -g phpt /tmp/perfidious-preload")
                machine1.succeed("runuser -u phpt -- cp -r --no-preserve=mode,ownership ${src}/tests ${php.unwrapped.dev}/lib/build/run-tests.php /tmp/perfidious-preload/")
                preload_command = (
                    "runuser -u phpt -- sh -c 'cd /tmp/perfidious-preload && "
                    "TEST_PHP_DETAILED=1 NO_INTERACTION=1 REPORT_EXIT_STATUS=1 "
                    "${lib.optionalString (php.extensions ? opcache) "PERFIDIOUS_TEST_OPCACHE=${php.extensions.opcache}/lib/php/extensions/opcache.so "}"
                    "php run-tests.php --show-diff -W results.txt "
                    "tests/request-handle/fpm-preload.phpt tests/request-handle/fpm-preload-disabled.phpt'"
                )
                preload_status, preload_output = machine1.execute(preload_command)
                (driver.out_dir / "phpt-preload.log").write_text(preload_output, encoding="utf-8")
                preload_results = machine1.succeed("cat /tmp/perfidious-preload/results.txt")
                (driver.out_dir / "phpt-preload-results.txt").write_text(preload_results, encoding="utf-8")
                assert preload_status == 0, preload_output
                assert sorted(preload_results.splitlines()) == [
                    "PASSED\t/tmp/perfidious-preload/tests/request-handle/fpm-preload-disabled.phpt",
                    "PASSED\t/tmp/perfidious-preload/tests/request-handle/fpm-preload.phpt",
                ], preload_results
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

        # Build Perfidious into PHP so CLI and FPM use the same instrumented extension.
        # Only Perfidious's objects are instrumented; PHP core links the sanitizer runtimes.
        # Rebuilding PHP is slow, so these targets are excluded from the normal package/check/shell
        # matrices and exposed separately below.
        sanitizeStdenv =
          if pkgs.stdenv.cc.isClang
          then pkgs.llvmPackages.stdenv
          else pkgs.stdenv;

        makeSanitizeStaticPhp = {
          debugSupport ? false,
          ztsSupport ? false,
        }: let
          basePhp = pkgs.php82.unwrapped;
        in
          pkgs.callPackage (nixpkgs + "/pkgs/development/interpreters/php/generic.nix") {
            stdenv = sanitizeStdenv;
            pcre2 = pkgs.pcre2.override {withJitSealloc = false;};
            version = basePhp.version;
            phpSrc = basePhp.src;
            inherit ztsSupport;
            # The threaded fixture uses CLI SAPI callbacks.
            cgiSupport = !ztsSupport;
            fpmSupport = !ztsSupport;
            phpdbgSupport = !ztsSupport;
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
                  # PHP core is not instrumented, so its loader needs the sanitizer-compatible
                  # flags explicitly when loading OpCache or a shared test helper.
                  substituteInPlace Zend/zend_portability.h \
                    --replace-fail 'PHP_RTLD_MODE | RTLD_GLOBAL | RTLD_DEEPBIND' \
                      'PHP_RTLD_MODE | RTLD_GLOBAL'
                '';
              configureFlags =
                prev.configureFlags
                ++ [
                  "--enable-perfidious"
                  "--enable-perfidious-sanitize"
                  "--enable-compile-warnings=yes"
                  "--disable-Werror"
                ]
                ++ lib.optional debugSupport "--enable-perfidious-debug";
              # only LDFLAGS (needed on the final link, to pull in the sanitizer runtime) applies
              # to the whole build - CFLAGS stays scoped to just perfidious's own object files,
              # via --enable-perfidious-sanitize above, see config.m4
              LDFLAGS = "${prev.LDFLAGS or ""} -fsanitize=address,undefined";
            };
          };

        sanitizeStaticPhp = makeSanitizeStaticPhp {};
        sanitizeStaticPhpDebug = makeSanitizeStaticPhp {debugSupport = true;};
        sanitizeStaticPhpZtsDebug = makeSanitizeStaticPhp {
          debugSupport = true;
          ztsSupport = true;
        };

        makeSanitizeStaticPhpCheck = {
          php,
          debugSupport ? false,
        }:
          pkgs.runCommand "perfidious-sanitize-static${lib.optionalString php.ztsSupport "-zts"}${lib.optionalString debugSupport "-debug"}-check" {
            nativeBuildInputs = [sanitizeStdenv.cc pkgs.php82 php.dev pkgs.python3];
          } ''
            cp -r --no-preserve=mode,ownership ${src}/tests .
            cp -r --no-preserve=mode,ownership ${src}/stubs .
            cp -r --no-preserve=mode,ownership ${src}/src .
            cp --no-preserve=mode,ownership ${src}/php_perfidious.h .
            cp ${php.dev}/lib/build/run-tests.php .

            export USE_ZEND_ALLOC=0
            export LD_PRELOAD="$(cc -print-file-name=libasan.so):$(cc -print-file-name=libubsan.so)"
            export ASAN_OPTIONS="detect_leaks=0:abort_on_error=1"
            export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"
            export NO_INTERACTION=1
            export REPORT_EXIT_STATUS=1
            export PATH=${php.dev}/bin:$PATH
            export PERFIDIOUS_STUB_PHP=${pkgs.php82}/bin/php
            ${
              if php.ztsSupport
              then ''
                export PERFIDIOUS_TEST_ZTS_BUILTIN=1
                export PERFIDIOUS_TEST_ZTS_SANITIZE=1
                ${php}/bin/php -n -r '
                  if (!PHP_ZTS) {
                    fwrite(STDERR, "ZTS PHP is required for this check\n");
                    exit(1);
                  }
                '
              ''
              else ''
                export PERFIDIOUS_TEST_FPM_BUILTIN=1
                export PERFIDIOUS_TEST_OPCACHE=${pkgs.php82.extensions.opcache}/lib/php/extensions/opcache.so
              ''
            }

            ${lib.optionalString debugSupport ''
              ${php}/bin/php -n -r '
                if (!Perfidious\DEBUG) {
                  fwrite(STDERR, "Perfidious debug hooks are required for this check\n");
                  exit(1);
                }
              '
            ''}
            ${php}/bin/php -n run-tests.php \
              || (find tests -name '*.log' | xargs -r cat; exit 1)

            touch $out
          '';

        sanitizeStaticPhpCheck = makeSanitizeStaticPhpCheck {php = sanitizeStaticPhp;};
        sanitizeStaticPhpDebugCheck = makeSanitizeStaticPhpCheck {
          php = sanitizeStaticPhpDebug;
          debugSupport = true;
        };
        sanitizeStaticPhpZtsDebugCheck = makeSanitizeStaticPhpCheck {
          php = sanitizeStaticPhpZtsDebug;
          debugSupport = true;
        };
      in {
        apps.generate-arginfo = {
          type = "app";
          program = "${generateArginfo}/bin/perfidious-generate-arginfo";
        };

        # Sanitizer targets are added only here, to the *returned*
        # packages set, not the `packages` variable devShells/checks below are built from - see
        # makeSanitizeStaticPhp's comment above.
        packages =
          packages
          // {
            sanitize-static-php82 = sanitizeStaticPhp;
            sanitize-static-php82-check = sanitizeStaticPhpCheck;
            sanitize-static-php82-debug = sanitizeStaticPhpDebug;
            sanitize-static-php82-debug-check = sanitizeStaticPhpDebugCheck;
            sanitize-static-php82-zts-debug = sanitizeStaticPhpZtsDebug;
            sanitize-static-php82-zts-debug-check = sanitizeStaticPhpZtsDebugCheck;
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
