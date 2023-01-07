Drawpile - a collaborative drawing program  (dev branch)
------------------------------------------

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
 * Rust compiler (tested with rustc 1.40)

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

Example:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make

The executables will be generated in the `build/bin` directory. You can run them from there,
or install them with `make install`.

The configuration step supports some options:

* `CLIENT=off`: don't build the client (useful when building the stand-alone server only)
* `SERVER=off`: don't build the stand-alone server.
* `SERVERGUI=off`: build a headless-only stand-alone serveer.
* `TOOLS=on`: build dprec2txt command line tool
* `CMAKE_BUILD_TYPE=debug`: enable debugging features
* `INITSYS=""`: select init system integration (currently only "systemd" is supported.) Set this to an empty string to disable all integration.
* `TESTS=on`: build unit tests (run test suite with `make test`)
* `KIS_TABLET=on`: enable improved graphics tablet support (taken from Krita)

Notes about `KIS_TABLET`:

 * On Windows, it enables Windows Ink and improved Wintab support. A patched version of Qt should be used. See `pkg/win` for the patch.
 * On Linux (or any platform with an X server,) it enables a modified XInput2 event handler.
 * On macOS it does nothing and shouldn't be used.

Example: `$ cmake .. -DCMAKE_BUILD_TYPE=debug -DKIS_TABLET=on`

For instructions on how to build Drawpile on Windows and OSX, see the [Building from sources] page.

[QtColorPicker]: https://gitlab.com/mattia.basaglia/Qt-Color-Widgets
[QtKeyChain]: https://github.com/frankosterfeld/qtkeychain
[Building from sources]: https://github.com/drawpile/Drawpile/wiki/Building-from-sources

