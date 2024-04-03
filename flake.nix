{
  description = "php-perf";

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
          name = "php-perf-source";
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
        }:
          pkgs.callPackage ./nix/derivation.nix {
            inherit src;
            inherit stdenv php libpfm;
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
            name = "php-perf-vm-test";
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
              machine.succeed("php -m | grep -i perf")
              machine.succeed("cp -r --no-preserve=mode,ownership ${src}/* .")
              machine.succeed("cp --no-preserve=mode,ownership ${php.unwrapped}/lib/build/run-tests.php .")
              machine.succeed("TEST_PHP_DETAILED=1 NO_INTERACTION=1 REPORT_EXIT_STATUS=1 php run-tests.php || (find tests -name '*.log' | xargs -n1 cat ; exit 1)")
            '';
          };

        packages = rec {
          php81 = makePackage {
            php = pkgs.php81;
          };
          php81-clang = makePackage {
            php = pkgs.php81;
            stdenv = pkgs.clangStdenv;
          };
          php81-libpfm-unstable = makePackage {
            php = pkgs.php81;
            libpfm = libpfm-unstable;
          };
          php82 = makePackage {
            php = pkgs.php82;
          };
          php82-clang = makePackage {
            php = pkgs.php82;
            stdenv = pkgs.clangStdenv;
          };
          php82-libpfm-unstable = makePackage {
            php = pkgs.php82;
            libpfm = libpfm-unstable;
          };
          php83 = makePackage {
            php = pkgs.php83;
          };
          php83-clang = makePackage {
            php = pkgs.php83;
            stdenv = pkgs.clangStdenv;
          };
          php83-libpfm-unstable = makePackage {
            php = pkgs.php83;
            libpfm = libpfm-unstable;
          };
          default = php81-libpfm-unstable;
        };
      in {
        inherit packages;

        devShells = builtins.mapAttrs (name: package: makeDevShell package) packages;

        checks =
          {inherit pre-commit-check;}
          // (builtins.mapAttrs (name: package: makeCheck package) packages)
          // (builtins.mapAttrs (name: package: makeVmCheck package) packages);

        formatter = pkgs.alejandra;
      }
    );
}
