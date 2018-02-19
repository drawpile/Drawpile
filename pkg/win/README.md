Building Windows package with docker
------------------------------------

First, create the build environment image:

    docker build -f Dockerfile -t dpwin8 .

Next, run the build script to build the installer

	./build.sh installer

Note. You must have wine and InnoSetup installed for this step.
Alternatively, you can run just

    ./build.sh pkg

to build a ZIP package.

The patch file `qtbase-2-no-tabletevent.patch' added before Qt is compiled.
This patch disables Qt's own tablet event handling so it can be replaced
at the application level. The custom tablet handling code is enabled by
passing `KIS_TABLET=on` to cmake.

