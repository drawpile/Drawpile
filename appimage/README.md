Building AppImages with docker
------------------------------

First, create the build environment image:

    docker build -f Dockerfile -t drawpile2 .

Next, run the build script to build the client and server AppImages:

	./build.sh

