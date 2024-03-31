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
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    gitignore,
    pre-commit-hooks,
    ...
  } @ args:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
        lib = pkgs.lib;

        src' = gitignore.lib.gitignoreSource ./.;

        iwyu = pkgs.callPackage ./nix/iwyu.nix {};

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

        makePhp = {php}:
          php.buildEnv {
            extensions = {
              enabled,
              all,
            }:
              enabled ++ [all.opcache];
          };

        makePackage = {php}:
          pkgs.callPackage ./nix/derivation.nix {
            php = makePhp {
              inherit php;
            };
            inherit (php) buildPecl;
            inherit src;
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
            shellcheck.enable = true;
          };
        };

        mkDevShell = package:
          pkgs.mkShell {
            inputsFrom = [package];
            buildInputs = with pkgs; [
              actionlint
              autoconf-archive
              clang-tools
              iwyu
              lcov
              linuxPackages_latest.perf
              gdb
              package.php
              package.php.packages.composer
              valgrind
            ];
            shellHook = ''
              ${pre-commit-check.shellHook}
              mkdir -p .direnv/include
              ln -sf ${package.php.unwrapped.dev}/include/php/ .direnv/include/php
              ln -sf ${package.libpfm}/include/perfmon .direnv/include/perfmon
              export REPORT_EXIT_STATUS=1
              export NO_INTERACTION=1
              export PATH="$PWD/vendor/bin:$PATH"
              # opcache isn't getting loaded for tests because tests are run with '-n' and nixos doesn't compile
              # in opcache and relies on mkWrapper to load extensions
              export TEST_PHP_ARGS='-c ${package.php.phpIni}'
            '';
          };

        makeVmTest = package: let
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
              machine.succeed("php -d perf.enable=1 -r 'var_dump(\PerfExt\perf_stat());'")
              machine.succeed("cp -r --no-preserve=mode,ownership ${src}/* .")
              machine.succeed("cp --no-preserve=mode,ownership ${php.unwrapped}/lib/build/run-tests.php .")
              machine.succeed("TEST_PHP_DETAILED=1 NO_INTERACTION=1 REPORT_EXIT_STATUS=1 php run-tests.php || (cat tests/*.log ; exit 1)")
            '';
          };
      in rec {
        packages = rec {
          php81 = makePackage {
            php = pkgs.php81;
          };
          php82 = makePackage {
            php = pkgs.php82;
          };
          php83 = makePackage {
            php = pkgs.php83;
          };
          default = php81;
        };

        devShells = rec {
          php81 = mkDevShell packages.php81;
          php82 = mkDevShell packages.php82;
          php83 = mkDevShell packages.php83;
          default = php81;
        };

        checks = rec {
          inherit pre-commit-check;
          php81 = makeCheck packages.php81;
          php82 = makeCheck packages.php82;
          php83 = makeCheck packages.php83;
          vm81 = makeVmTest packages.php81;
          vm82 = makeVmTest packages.php82;
          vm83 = makeVmTest packages.php83;
        };

        formatter = pkgs.alejandra;
      }
    );
}
