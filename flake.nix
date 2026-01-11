{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { nixpkgs, ... }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    in
    {
      devShells = forAllSystems (system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        {
          # default = pkgs.mkShell.override { stdenv = pkgs.clangStdenv; } {
          default = pkgs.mkShellNoCC {
            packages = with pkgs; [
              clang-tools
              clang
              cmake
              pkg-config
              wayland-scanner
              wayland
            ];
          };
        }
      );
    };
}
