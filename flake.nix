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
        "x86_64-darwin"
        "aarch64-darwin"
      ];

      imports = [ treefmt-nix.flakeModule ];

      perSystem =
        { pkgs, config, ... }:
        let
          llvm = pkgs.llvmPackages_22;
        in
        {
          # clang with each platform's own standard library, so it links nix's c++ libraries
          devShells.default = pkgs.mkShell.override { inherit (llvm) stdenv; } {
            packages = with pkgs; [
              config.treefmt.build.wrapper
              llvm.clang-tools
              llvm.lldb
              cmake
              cmake-language-server
              ninja
              pkg-config
              just
              doctest
              protobuf
            ];

            CMAKE_EXPORT_COMPILE_COMMANDS = "1";
            CMAKE_GENERATOR = "Ninja";
            CC = "clang";
            CXX = "clang++";
          };

          treefmt = {
            projectRootFile = "flake.nix";
            settings.global.excludes = [ "build/**" ];
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
