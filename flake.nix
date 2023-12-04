# TODO: Consider using mold (linker)
#TODO: Add declarative drawpile server support
#TODO: Consider my terrible life choices that led me to derive os-es from config files
{
  description = "Collaborative drawing program that lets multiple people draw.";

  inputs = {
    #main packages
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    #utilities in writing flakes
    flake-utils.url = "github:numtide/flake-utils";

    #compatibility with non flaked nix
    flake-compat.url = "https://flakehub.com/f/edolstra/flake-compat/1.tar.gz";
  };

  outputs = { self, nixpkgs, flake-utils, ... }:
    #Support x86_64/arm linux
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};

        #Compile time libs

        compileTimeDeps = with pkgs; [ cmake ];

        devOnlyDepends = with pkgs; [
          rustfmt
          clippy
          
          # We use rust as devOnly dependency
          # Since rustPlatform.buildRustPackage takes care of rust in compile time enviroment
          # (we use buildRustPackage so we do not have to deal with pain of deravations being offline)
          rustc
          cargo
        ];

        # QT depdnencies
        qt6Dependencies = with pkgs; [
          qt6.qtbase
          qt6.qtsvg
          qt6.qttools
          qt6.qtmultimedia
        ];

        qt5Dependencies = with pkgs; [
          libsForQt5.karchive
          qt5.qtbase
          qt5.qtsvg
          qt5.qttools
          qt5.qtmultimedia
        ];

        otherDeps = with pkgs; [
          rustc
          cargo

          cmake

          git
          libxkbcommon
          gcc
          libzip
          libsodium
          libmicrohttpd
        ];
      in rec {
        mkDrawpile = { buildClient ? true, buildServer ? true, useQt5 ? false
          , buildType ? "Release", }:
          let
            qtDependencies =
              if useQt5 then qt5Dependencies else qt6Dependencies;

            clientBuildString = if buildClient then "ON" else "OFF";
            serverBuildString = if buildServer then "ON" else "OFF";

          in pkgs.rustPlatform.buildRustPackage {
            name = "drawpile";
            src = self;

            nativeBuildInputs = with pkgs;
              [ qt6.wrapQtAppsHook wrapGAppsHook ] ++ compileTimeDeps;

            buildInputs = otherDeps ++ qtDependencies;

            cargoLock = { lockFile = ./Cargo.lock; };

            configurePhase = ''
              cmake -S ./ -B Drawpile-build \
                -DCMAKE_BUILD_TYPE=${buildType} \
                -DCLIENT=${clientBuildString} \
                -DSERVER=${serverBuildString} \
                -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
                -DCMAKE_INSTALL_PREFIX=$out
            '';

            enableParallelBuilding = true;

            buildPhase = ''
              cmake --build ./Drawpile-build
            '';

            installPhase = ''
              mkdir -p $out
              cmake --install ./Drawpile-build
            '';
          };

        packages = rec {
          drawpile-full = mkDrawpile { };
          drawpile-full-qt5 = mkDrawpile { useQt5 = true; };

          drawpile-client-only = mkDrawpile { buildServer = false; };
          drawpile-server-only = mkDrawpile { buildClient = false; };

          default = drawpile-full;
        };

        #Dev shells

        devShells = rec {
          qt6-shell = with pkgs;
            mkShell {
              nativeBuildInputs = devOnlyDepends ++ compileTimeDeps;

              buildInputs = qt6Dependencies ++ otherDeps;
            };

          qt5-shell = with pkgs;
            mkShell {
              nativeBuildInputs = devOnlyDepends ++ compileTimeDeps;

              buildInputs = qt5Dependencies ++ otherDeps;
            };

          default = qt6-shell;
        };

        formatter = pkgs.nixfmt;
      });
}
