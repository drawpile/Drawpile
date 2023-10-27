# Drawpile - A Collaborative Drawing Program

[![CI status badge](../../actions/workflows/main.yml/badge.svg)](../../actions/workflows/main.yml) [![translation status](https://hosted.weblate.org/widgets/drawpile/-/svg-badge.svg)](https://hosted.weblate.org/engage/drawpile/)

Drawpile is a drawing program that lets you share the canvas
with other users in real time.

Some feature highlights:

* Runs on Linux, Windows, macOS, and Android
* Shared canvas using the built-in server or a dedicated server
* Record, play back and export drawing sessions
* Animation support
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

Building software on Windows is a pretty miserable experience. It usually takes at least 3 hours for the development environment to finish installing, possibly more depending on the speed of your computer and network. You should also have at least 50 GB of free disk space, otherwise the process may fail.

1. Install Rust and Visual Studio Community Edition. Download [the Rust installer](https://static.rust-lang.org/rustup/dist/x86_64-pc-windows-msvc/rustup-init.exe) and run it. You can leave everything on the default options to let in install everything that's needed.
1. Run the Visual Studio x64 Command Line. You should be able to find it in your start menu by searching for "x64". You a black window with white in it.
   1. If you haven't used the Windows command line before: it's weird and all kinds of terrible.
   1. **Do not press `Ctrl+V`**, it won't paste, it will just write garbage that you have to delete or else it will mess things up. To paste something, you have to use `Shift+Insert` instead.
   1. **Do not click around in the window.** Only click the title bar or the entry in the task bar. Else it will get stuck until you press enter - but it won't tell you that, so you'll just be waiting for it to do something while it's waiting for you to press a key for all eternity.
   1. **Don't try to copy stuff out of the window.** That will almost always get it stuck and/or kill whatever is currently running. Just take a screenshot instead if you want to show someone what it says.
1. Install vcpkg:
   1. Run `cd /D C:\` (type it into the command line window and hit enter) to switch to the root of the C drive.
   1. Then run `git clone https://github.com/Microsoft/vcpkg.git` to get vcpkg.
   1. Then `.\vcpkg\bootstrap-vcpkg.bat -disableMetrics` to set up vcpkg.
1. Install the dependencies:
   1. This step takes really long to complete. Again, make sure you have at least 50 GB of free disk space.
   1. Run `vcpkg --disable-metrics install --clean-after-build qt5-base:x64-windows qt5-multimedia:x64-windows qt5-svg:x64-windows qt5-tools:x64-windows qt5-translations:x64-windows kf5archive:x64-windows libmicrohttpd:x64-windows libsodium:x64-windows qtkeychain:x64-windows` to install the dependencies.
   1. Go do something else for the next several hours while it installs everything.
1. Build Drawpile:
   1. Run `cd /D %HOMEDRIVE%%HOMEPATH%` to switch to your home directory. It is probably under `C:\Users\<YOUR NAME>`.
   2. Then run `git clone https://github.com/drawpile/Drawpile.git` to get Drawpile's source code.
   3. Then `cd Drawpile` to switch into that directory.
   4. Configure the build: `cmake -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug> -DCLIENT=ON -DSERVER=ON -DSERVERGUI=ON -DTOOLS=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=install -DX_VCPKG_APPLOCAL_DEPS_INSTALL=ON -B build`
   5. Build it: `cmake --build build` - this takes a few minutes the first time around.
   6. Install it: `cmake --install build` - this won't overwrite any existing installation of Drawpile, it will just put the stuff you built and the DLLs, icons  and such together so that you can actually run it.
1. Run what you built. The executable should be at `C:\Users\<YOUR NAME>\Drawpile\install\drawpile.exe`

This will build in debug mode, which is meant for development and runs slowly. If you want a fully-optimized release build, you have to run the configure step above with `-DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON` instead of `-DCMAKE_BUILD_TYPE=Debug`.

After you make changes to the code, you have to run the build and install steps again. You don't have to re-run the configure step.

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
