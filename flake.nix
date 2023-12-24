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
          cmake

          git
          libxkbcommon
          gcc
          libzip
          libsodium
          libmicrohttpd
        ];

        mkDrawpile = { useQt5, preset, debug ? false }:
          let

            qtDependencies =
              if useQt5 then qt5Dependencies else qt6Dependencies;

          in pkgs.rustPlatform.buildRustPackage {
            name = "drawpile";
            src = self;

            nativeBuildInputs = with pkgs;
              [ qt6.wrapQtAppsHook wrapGAppsHook ] ++ compileTimeDeps;

            buildInputs = with pkgs;
              [ git libxkbcommon libzip libsodium libmicrohttpd ]
              ++ qtDependencies
              ++ (if debug then with pkgs; [ clippy rustfmt ] else [ ]);

            cargoLock = { lockFile = ./Cargo.lock; };

            configurePhase = ''
              cmake -S ./ -B Drawpile-build \
                --preset ${preset} \
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

      in rec {

        packages = rec {
          debug-qt6-all-make = mkDrawpile {
            preset = "linux-debug-qt6-all-make";
            useQt5 = false;
          };
          release-qt6-all-make = mkDrawpile {
            preset = "linux-release-qt6-all-make";
            useQt5 = false;
          };
          release-qt6-server-make = mkDrawpile {
            preset = "linux-release-qt6-server-make";
            useQt5 = false;
          };

          debug-qt5-all-make = mkDrawpile {
            preset = "linux-debug-qt5-all-make";
            useQt5 = true;
          };
          release-qt5-all-make = mkDrawpile {
            preset = "linux-release-qt5-all-make";
            useQt5 = true;
          };
          release-qt5-server-make = mkDrawpile {
            preset = "linux-release-qt5-server-make";
            useQt5 = true;
          };

          default = release-qt6-all-make;
        };

        #Dev shells

        devShells = rec {
          debug-qt6-all-make = mkDrawpile {
            preset = "linux-debug-qt6-all-make";
            useQt5 = false;
            debug = true;
          };
          release-qt6-all-make = mkDrawpile {
            preset = "linux-release-qt6-all-make";
            useQt5 = false;
            debug = true;
          };
          release-qt6-server-make = mkDrawpile {
            preset = "linux-release-qt6-server-make";
            useQt5 = false;
            debug = true;
          };

          debug-qt5-all-make = mkDrawpile {
            preset = "linux-debug-qt5-all-make";
            useQt5 = true;
            debug = true;
          };
          release-qt5-all-make = mkDrawpile {
            preset = "linux-release-qt5-all-make";
            useQt5 = true;
            debug = true;
          };
          release-qt5-server-make = mkDrawpile {
            preset = "linux-release-qt5-server-make";
            useQt5 = true;
            debug = true;
          };

          default = release-qt6-all-make;
        };

        formatter = pkgs.nixfmt;
      });
}
