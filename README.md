Drawpile - a collaborative drawing program
------------------------------------------

DrawPile is a drawing program with a twist: you can share your drawing
live with other people

Some feature highlights:

* Shared network drawing
* Built-in and standalone servers
* Layers 
* Text annotations
* Tablet support
* Chat
* Canvas rotation
* Supports OpenRaster file format

## Building with cmake

Dependencies:

* Qt 5.0 or newer
* zlib

It's a good idead to build in a separate directory to keep build files
separate from the source tree.

Example:

    $ mkdir build
    $ cd build
    $ cmake ..
    $ make

The executables will be generated in the build/bin directory. You can run them from there,
or install them with `make install`.

The configuration step supports some options:

* CLIENT=off: don't build the client (useful when building the stand-alone server only)
* SERVER=off: don't build the stand-alone server.
* DEBUG=on: enable debugging features

Example:
    $ cmake .. -DDEBUG=on

