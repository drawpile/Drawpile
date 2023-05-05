# Drawpile - A Collaborative Drawing Program

[![CI status badge](../../actions/workflows/main.yml/badge.svg)](../../actions/workflows/main.yml) [![translation status](https://hosted.weblate.org/widgets/drawpile/-/svg-badge.svg)](https://hosted.weblate.org/engage/drawpile/)

Drawpile is a drawing program that lets you share the canvas
with other users in real time.

Some feature highlights:

* Runs on Linux, Windows, macOS, and Android
* Shared canvas using the built-in server or a dedicated server
* Record, play back and export drawing sessions
* Simple animation support
* Layers and blending modes
* Text layers
* Supports pressure sensitive Wacom tablets
* Built-in chat
* Supports OpenRaster file format
* Encrypted connections using SSL
* MyPaint brush support

We use [Weblate](https://hosted.weblate.org/engage/drawpile/) for translations.

[![translation status](https://hosted.weblate.org/widgets/drawpile/-/287x66-grey.png)](https://hosted.weblate.org/engage/drawpile/)

## Installing Drawpile

Precompiled releases are available from the [Drawpile web site](https://drawpile.net/download/)
or from [GitHub releases](../../releases). Work-in-progress builds are available as
downloadable artefacts from [GitHub Actions](../../actions).

## Building Drawpile from source

The following dependencies are required:

* CMake 3.18 (3.19+ recommended)
* C++17 compiler
* Rust stable compiler
* Qt 5.12 or newer (5.11 is also supported for headless server only)

### Windows

Using Visual Studio:

1. Run the [Rust installer](https://static.rust-lang.org/rustup/dist/x86_64-pc-windows-msvc/rustup-init.exe),
   which will also automatically install Visual Studio Community Edition
1. Open the Visual Studio x64 command line from the Start menu and
   [install vcpkg](https://vcpkg.io/en/getting-started.html). Be sure to run
   the `vcpkg integrate install` step for integration with Visual Studio
1. Run
   `vcpkg install libzip:x64-windows qtbase:x64-windows qtmultimedia:x64-windows qtsvg:x64-windows qttools:x64-windows qttranslations:x64-windows`
   to install required dependencies, and
   `vcpkg install libmicrohttpd:x64-windows libsodium:x64-windows qtkeychain-qt6:x64-windows`
   to install optional dependencies
1. Run `git clone https://github.com/drawpile/Drawpile.git` to clone Drawpile
1. Open Visual Studio from the Start menu and open `Drawpile\CMakeLists.txt`
   and use normally. Note that because assets are not copied into the runtime
   directory, icons and brushes will be missing from the user interface. Copy
   the files from the `src\desktop\assets` directory into the directory with
   the built executable.

### macOS

Using Xcode:

1. [Install Xcode](https://apps.apple.com/us/app/xcode/id497799835?mt=12)
1. [Install Homebrew](https://brew.sh/)
1. [Install Rust](https://www.rust-lang.org/tools/install)
1. Open Terminal.app and run `brew install cmake libzip qt` to install required
   dependencies, and `brew install libmicrohttpd libsodium qtkeychain` to install
   optional dependencies
1. Run `git clone https://github.com/drawpile/Drawpile.git` to clone Drawpile
1. Run `cmake -S Drawpile -B Drawpile-build -G Xcode` to generate the Xcode
   project
1. Run `open Drawpile-build/Drawpile.xcodeproj` to open the project in Xcode
   and use normally

### Linux (Ubuntu)

1. Run
   `sudo apt install build-essential cmake git libqt6svg6 qt6-base-dev qt6-multimedia-dev qt6-tools-dev libxkbcommon-dev libzip-dev zipcmp zipmerge ziptool`
   to install required dependencies, and
   `sudo apt install libmicrohttpd-dev libsodium-dev libsystemd-dev qtkeychain-qt6-dev`
   to install optional dependencies
1. [Install Rust](https://www.rust-lang.org/tools/install)
1. Run `cmake -S Drawpile -B Drawpile-build -DCMAKE_BUILD_TYPE=RelWithDebInfo`
   to generate the project and follow the output instructions to build

### Manually compiled dependencies

Once CMake is installed, different versions of dependencies can be downloaded
and installed from source using the CI support scripts:

`cmake -DCMAKE_INSTALL_PREFIX=<installation path> -DQT_VERSION=<version> -P .github/scripts/build-qt.cmake`
`cmake -DCMAKE_INSTALL_PREFIX=<installation path> -P .github/scripts/build-other.cmake`

After installing dependencies from source, regenerate the project with
`-DCMAKE_PREFIX_PATH=<installation path>` to use the source dependencies.

The source dependency scripts can also be used to build dependencies that have
extra assertions and ASan by running them with `-DBUILD_TYPE=debug`. Note that
this still generates release binaries, just with extra instrumentation to find
runtime bugs.

Note that transient dependencies are not handled by these scripts; they are
intended primarily for use by the CI service and are provided as-is.

## Installing Drawpile from source

Follow the above instructions, optionally using
`-DCMAKE_INSTALL_PREFIX=<installation path>` when generating the project to
install to a directory other than the system root, then run `cmake --build`
as usual, and finally `cmake --install`.
