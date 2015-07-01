Drawpile - a collaborative drawing program
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

## Building with cmake

Client dependencies:

* Qt 5.0 or newer
* [KF5 KArchive]
* KF5 KDNSSD (optional)
* GIFLIB (optional)
* [QtColorPicker]
* MiniUPnP (optional)

Server dependencies:

* Qt 5.0 or newer (QtCore and QtNetwork only)
* [KF5 KArchive]
* libsystemd (optional)
* libmicrohttpd (optional)

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

* CLIENT=off: don't build the client (useful when building the stand-alone server only)
* SERVER=off: don't build the stand-alone server.
* TOOLS=on: build dprec2txt command line tool
* CMAKE\_BUILD\_TYPE=debug: enable debugging features
* INITSYS="": select init system integration (currently only "systemd" is supported.) Set this to an empty string to disable all integration.

Example: `$ cmake .. -DCMAKE_BUILD_TYPE=debug`

For instructions on how to build Drawpile on Windows and OSX, see the [Building from sources] page.

[KF5 KArchive]: https://projects.kde.org/projects/frameworks/karchive
[QtColorPicker]: https://github.com/mbasaglia/Qt-Color-Picker 
[Building from sources]: https://github.com/callaa/Drawpile/wiki/Building-from-sources

