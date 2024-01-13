# Docker Drawpile Server

[Docker](https://www.docker.com/) is a way to easily package applications into shippable 'containers'.
This directory contains the Dockerfile defintion for creating a drawpile server. 

A ready to use docker image is published at Docker Hub. To get started, you can simply run:

    docker run -it --rm -p 27750:27750 askmeaboutloom/drawpile-srv:2.2.0

To build the image yourself, simply run the following command in this directory:

    docker build -t drawpile-srv .

Which will download and compile the latest Drawpile server. You can specify the version
to build with `--build-arg=version=2.2.0` (where 2.2.0 is the git tag or branch you wish to build.)
If no version is specified, the latest main branch version is built.

At the end of this process you will have a Docker container image ready to be run.

To run the your Drawpile docker container, simply use the following command:

    docker run -dt -p 27750:27750 drawpile-srv

This will start a drawpile server in the background running on the default port 27750.
You can also specify `-p 127.0.0.1:27780:27780` and `--web-admin-port 27780 --web-admin-access all` to expose the web admin port.

Typically, you will also want to use file backed sessions and a configuration database.
Example usage:

    docker run -dt --name "drawpile-server" \
        -p 27750:27750 -p 127.0.0.1:27780:27780 \
        -v dpsessions:/home/drawpile \
        --restart always \
        drawpile-srv:2.2.0 --sessions /home/drawpile/sessions \
        --database /home/drawpile/config.db \
        --web-admin-port 27780 --web-admin-access all

The above does the following things:

 * Create a container named `drawpile-server` from the `drawpile-srv:2.2.0` image and run in in the background
 * Publish port 27750 on all interfaces and 27780 on loopback only
 * Mount the named volume `dpsessions` at `/home/drawpile`
 * Restart the container if it crashes or the server is restarted
 * Start drawpile server using file backed sessions, database config and web admin enabled
 * Note: web admin access is limited to localhost by the `127.0.0.1:27780:27780` mapping, but `all` access should be used inside the container

