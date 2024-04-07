{
  description = "php-perfidious";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-23.11";
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
      inputs.flake-utils.follows = "flake-utils";
      inputs.gitignore.follows = "gitignore";
    };
    nix-github-actions = {
      url = "github:nix-community/nix-github-actions";
      inputs.nixpkgs.follows = "nixpkgs";
    };
    libpfm4-unstable-src = {
      url = "git+https://git.code.sf.net/p/perfmon2/libpfm4";
      flake = false;
    };
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    gitignore,
    pre-commit-hooks,
    systems,
    nix-github-actions,
    libpfm4-unstable-src,
    ...
  } @ args:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
        lib = pkgs.lib;

        src' = gitignore.lib.gitignoreSource ./.;

        iwyu = pkgs.callPackage ./nix/iwyu.nix {};

        libpfm-unstable = pkgs.libpfm.overrideAttrs (o: {
          version = "${o.version}+git-master";
          src = libpfm4-unstable-src;
        });

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
            clang-format.enable = true;
            clang-format.types_or = ["c" "c++"];
            clang-format.files = "\\.(c|h)$";
            markdownlint.enable = true;
            markdownlint.excludes = ["LICENSE\.md"];
            markdownlint.settings.configuration = {
              MD013 = {
                line_length = 1488;
                # this doesn't seem to work
                table = false;
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
              linuxPackages_latest.perf
              gdb
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
              enabled ++ [all.opcache package];
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
              machine.succeed("cp --no-preserve=mode,ownership ${php.unwrapped}/lib/build/run-tests.php .")
              machine.succeed("TEST_PHP_DETAILED=1 NO_INTERACTION=1 REPORT_EXIT_STATUS=1 php run-tests.php || (find tests -name '*.log' | xargs -n1 cat ; exit 1)")
            '';
          };

        matrix = with pkgs; {
          php = {
            inherit php81 php82 php83;
          };
          stdenv = {
            gcc = stdenv;
            clang = clangStdenv;
            musl = pkgsMusl.stdenv;
          };
          libpfm = {
            inherit libpfm libpfm-unstable;
          };
        };

        # @see https://github.com/NixOS/nixpkgs/pull/110787
        buildConfs =
          (lib.cartesianProductOfSets {
            php = ["php81" "php82" "php83"];
            stdenv = [
              "gcc"
              "clang"
              # totally broken
              # "musl"
            ];
            libpfm = ["libpfm" "libpfm-unstable"];
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
          ++ (lib.cartesianProductOfSets {
            php = ["php81" "php82" "php83"];
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
            default = packages.php81-gcc;
          };
      in {
        inherit packages;

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
