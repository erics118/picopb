{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    inputs@{ flake-parts, treefmt-nix, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "aarch64-darwin"
      ];

      imports = [ treefmt-nix.flakeModule ];

      perSystem =
        { pkgs, config, ... }:
        let
          llvm = pkgs.llvmPackages_22;

          shell = {
            # take the inputs from the clang check
            inputsFrom = [ config.checks.clang ];
            packages = with pkgs; [
              config.treefmt.build.wrapper
              llvm.clang-tools
              llvm.lldb
              cmake-language-server
              just
            ];

            CMAKE_GENERATOR = "Ninja";

            # disable safety flag that interferes with ubsan, that nix adds by default
            hardeningDisable = [ "strictoverflow" ];
          };

          src = pkgs.lib.fileset.toSource {
            root = ./.;
            fileset = pkgs.lib.fileset.unions [
              ./CMakeLists.txt
              ./cmake
              ./include
              ./tests
              ./examples
            ];
          };

          # build picopb and run ctest
          tests =
            name: stdenv: cmakeFlags:
            stdenv.mkDerivation {
              inherit name cmakeFlags src;
              inherit (shell) hardeningDisable;
              nativeBuildInputs = with pkgs; [
                cmake
                ninja
                protobuf
              ];
              buildInputs = with pkgs; [
                doctest
                protobuf
              ];
              doCheck = true;
              installPhase = "touch $out";
            };
        in
        {
          # default build with clang on macos and linux
          devShells.default = pkgs.mkShell.override { inherit (llvm) stdenv; } shell;

          # gcc for linux only
          # on macos, nix's libprotobuf uses libc++, while gcc uses libstdc++
          devShells.gcc = pkgs.mkShell.override { stdenv = pkgs.gccStdenv; } shell;

          checks = {
            clang = tests "picopb-clang" llvm.stdenv [ ];
            sanitize = tests "picopb-sanitize" llvm.stdenv [ "-DPICOPB_SANITIZE=ON" ];

            # build the example program for the rp2040 with gcc-arm-embedded
            # no tests because libprotobuf doesn't exist for rp2040
            arm = pkgs.stdenvNoCC.mkDerivation {
              name = "picopb-arm";
              inherit src;
              nativeBuildInputs = with pkgs; [
                cmake
                ninja
                gcc-arm-embedded
              ];
              cmakeFlags = [
                "-DCMAKE_TOOLCHAIN_FILE=${./cmake/rp2040.cmake}"
                "-DBUILD_TESTING=OFF"
              ];
              installPhase = "install -D picopb-example $out/bin/picopb-example";
            };
          }
          # gcc for linux only
          // pkgs.lib.optionalAttrs pkgs.stdenv.hostPlatform.isLinux {
            gcc = tests "picopb-gcc" pkgs.gccStdenv [ ];
          };

          treefmt = {
            projectRootFile = "flake.nix";
            programs = {
              clang-format.enable = true;
              nixfmt.enable = true;
              just.enable = true;
            };
          };

          formatter = config.treefmt.build.wrapper;
        };
    };
}
