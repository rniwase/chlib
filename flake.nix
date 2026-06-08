{
  description = "CH559 development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };

      in {
        packages.default = pkgs.stdenv.mkDerivation {
          pname = "chlib-midi-examples";
          version = "0.1";

          src = ./.;

          nativeBuildInputs = with pkgs; [
            sdcc
            gnumake
          ];

          dontConfigure = true;

          buildPhase = ''
            runHook preBuild

            make clean
            make build

            runHook postBuild
          '';

          installPhase = ''
            runHook preInstall

            mkdir -p $out

            shopt -s globstar nullglob

            found=0
            for f in **/*.bin; do
              cp "$f" "$out/"
              found=1
            done

            if [ "$found" -eq 0 ]; then
              echo "No .bin files found"
              exit 1
            fi

            runHook postInstall
          '';
        };

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            sdcc
            gnumake
            wchisp
          ];

          shellHook = ''
            echo "Entering CH559 development shell"
          '';
        };
      });
}
