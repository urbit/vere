{
  description = "default devshell";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
    # nixops-flake.url = "github:input-output-hk/nixops-flake";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
    # nixops-flake,
    ...
  }:
    flake-utils.lib.eachDefaultSystem
    (
      system: let
        pkgs = nixpkgs.legacyPackages.${system};
      in {
        devShell = pkgs.mkShell {
          buildInputs = with pkgs; [
            clang-tools
            zig
            zls
            python313

            # nixops-flake.defaultPackage.${system}
          ];
        };
      }
    );
}
