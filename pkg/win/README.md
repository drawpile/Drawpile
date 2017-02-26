Building Windows package with docker
------------------------------------

First, create the build environment image:

    docker build -f Dockerfile-32 -t dpwin8_32 .

Next, run the build script to build the installer:

    ./build.sh installer

Note. You must have wine and InnoSetup installed for this step.
Alternatively, you can build just a ZIP package by running:

    ./build.sh pkg

The patch files `qtbase-2-disable-wintab.patch` and `qtbase-3-disable-winink.patch`
are added before Qt is compiled.
These patches disable Qt's own tablet event handling so it can be replaced
at the application level. The custom tablet handling code is enabled by
passing `KIS_TABLET=on` to cmake.
Like the custom tablet code, these patches were copied from Krita. (`3rdparty/ext_qt`)

