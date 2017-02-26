Building Windows package with docker
------------------------------------

First, create the build environment image:

    docker build -f Dockerfile -t dpwin32 .

Next, run the build script to build the installer

	./build.sh installer

Note. You must have wine and InnoSetup installed for this step.
Alternatively, you can run just

    ./build.sh pkg

to build a ZIP package.

