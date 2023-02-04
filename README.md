# Drawpile - a collaborative drawing program (dev branch)

Drawpile is a drawing program that lets you share the canvas
with other users in real time.

Some feature highlights:

* Runs on Linux, Windows and OSX
* Shared canvas using the built-in server or a dedicated server
* Record, play back and export drawing sessions
* Simple animation support
* Layers and blending modes
* Text layers
* Supports pressure sensitive Wacom tablets
* Built-in chat
* Supports OpenRaster file format
* Encrypted connections using SSL
* Automatic port forwarding with UPnP
* MyPaint brush support

## Building with cmake

Common dependencies:
 * Qt 5.12 or newer (QtGui not required for headless server)
 * KF5 Extra CMake Modules
 * KF5 Archive (Qt5) or libzip (Qt6)

Client specific dependencies:

* [QtColorPicker]: optional, bundled copy is included
* [QtKeyChain]: optional, enables password storage
* KF5 KDNSSD: optional, local server discovery with Zeroconf

Server specific dependencies (you can also take a look at [Docker build](pkg/docker/Dockerfile) script):

* libsystemd: optional, systemd socket activation support
* libmicrohttpd: optional, HTTP admin API
* libsodium: optional: ext-auth support

It's a good idea to build in a separate directory to keep build files
separate from the source tree.

The configuration step supports some options:

* `DRAWDANCE_EXPORT_PATH=path/to/Drawdance.cmake`: path to the Drawdance.cmake file in the Drawdance build directory (required)
* `QT_VERSION=6`: use Qt6 instead of Qt5
* `CLIENT=OFF`: don't build the client (useful when building the stand-alone server only)
* `SERVER=OFF`: don't build the stand-alone server.
* `SERVERGUI=OFF`: build a headless-only stand-alone serveer.
* `TOOLS=ON`: build command line tools
* `CMAKE_BUILD_TYPE=Debug`: enable debugging features
* `INITSYS=""`: select init system integration (currently only "systemd" is supported.) Set this to an empty string to disable all integration.
* `TESTS=ON`: build unit tests (run test suite with `make test`)
* `KIS_TABLET=ON`: enable improved graphics tablet support from Krita. Only available on Windows with Qt5 and should be used with a patched version of Qt 5.12. The MSVC build takes care of setting this up.

For instructions on how to build Drawpile on Windows and OSX, see the [Building from sources] page.

[QtColorPicker]: https://gitlab.com/mattia.basaglia/Qt-Color-Widgets
[QtKeyChain]: https://github.com/frankosterfeld/qtkeychain

### Building on Linux

This is how to build on a plain Ubuntu 22.04. Other Linuxes are similar, the dependencies just have different names. There's two options: Qt6 or Qt5. Pick one of them, don't install both. If you're using an older version of your distribution, Qt6 might not yet be available for you.

Here's how to build with Qt6:

```sh
# Install dependencies (Qt6)
apt install -y build-essential cmake extra-cmake-modules git libgles2-mesa-dev libjpeg-dev libmicrohttpd-dev libpng-dev libsodium-dev libqt6svg6-dev libvulkan-dev libxkb-common-dev libz-dev libzip-dev ninja-build qtkeychain-qt6-dev qt6-multimedia-dev qt6-base-dev qt6-l10n-tools qt6-tools-dev qt6-tools-dev-tools zipcmp zipmerge ziptool

# Clone the repositories and check out the correct branches.
git clone --branch dev-2.2 https://github.com/drawpile/Drawdance.git
git clone --branch dancepile-2.2 https://github.com/askmeaboutlo0m/Drawpile.git

# Build Drawdance
cd Drawdance
scripts/cmake-presets drawpile_qt6_release
cmake --build builddpqt6release

# Build Drawpile
cd ../Drawpile
scripts/cmake-presets drawpile_qt6_release
cmake --build builddpqt6release

# Install Drawpile - this is required to be done at least once! Do NOT just run
# Drawpile from the build directory, you'll be missing brushes, themes and other
# stuff. If you did it anyway, installing won't fix it, you'll have to delete
# ~/.local/share/drawpile/drawpile and maybe ~/.config/drawpile/drawpile.ini.
sudo cmake --install builddpqt6release

# Run Drawpile
drawpile
```

And here with Qt5:

```sh
# Install dependencies (Qt5)
apt install -y build-essential cmake extra-cmake-modules git libjpeg-dev libkf5archive-dev libmicrohttpd-dev libpng-dev libsodium-dev libqt5svg5-dev libz-dev ninja-build qt5keychain-dev qt5multimedia-dev qtbase5-dev qttools5-dev

# Clone the repositories and check out the correct branches.
git clone --branch dev-2.2 https://github.com/drawpile/Drawdance.git
git clone --branch dancepile-2.2 https://github.com/askmeaboutlo0m/Drawpile.git

# Build Drawdance
cd Drawdance
scripts/cmake-presets drawpile_release
cmake --build builddprelease

# Build Drawpile
cd ../Drawpile
scripts/cmake-presets drawpile_release
cmake --build builddprelease

# Install Drawpile - this is required to be done at least once! Do NOT just run
# Drawpile from the build directory, you'll be missing brushes, themes and other
# stuff. If you did it anyway, installing won't fix it, you'll have to delete
# ~/.local/share/drawpile/drawpile and maybe ~/.config/drawpile/drawpile.ini.
sudo cmake --install builddprelease

# Run Drawpile
drawpile
```

### Building on Windows

Clone Drawpile and check out branch `dancepile-2.2`. Clone [Drawdance](https://github.com/drawpile/Drawdance) and check out branch `dev-2.2`. Put those two directories next to each other.

Install [vcpkg](https://github.com/microsoft/vcpkg) according to their instructions. Put it into `C:\vcpkg`, otherwise the paths might get too long and you will get strange errors.

Run the `x64 Native Tools Command Prompt`. Change to the `Drawpile/pkg/win-msvc` directory, something like:

    cd C:\Users\YourUsername\SomeOtherFoldersMaybe\Drawpile\pkg\win-msvc

Then run the build script:

    powershell -ExecutionPolicy ByPass -File build-release.ps1 -vcpkgDir C:\vcpkg

This will install all required dependencies, which takes several hours the first time. Make sure you have plenty of hard disk space (50 GB or more) for temporary files. Afterwards, it will build Drawdance and Drawpile.

Finally, package it up:

    powershell -ExecutionPolicy ByPass -File package.ps1 -vcpkgDir C:\vcpkg

Now you can run Drawpile from the newly generated `drawpile-X.Y.Z` directory in `Drawpile/pkg/win-msvg`. There will also be a ZIP file and, if you have InnoSetup available, an installer.

Note that the default PowerShell ZIP archive command is really old and broken, it generates ZIP files with `\` as a path separator, which is wrong and doesn't work on non-Windows operating systems. If you want to fix that, run `Install-Module -Name Microsoft.PowerShell.Archive -RequiredVersion 1.2.3.0` in PowerShell.
