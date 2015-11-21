# Docker Drawpile

Docker is a way to easily package applications into shippable 'containers'
which can be run on any server running a docker container. 

This directory contains the Dockerfile defintion for creating a drawpile
server from docker. 

To build, simply run the following commands:

`docker build -t drawpile .`

And to run:

`docker run --daemonize --tty -p 8080:8080 drawpile` 

This will start a drawpile server running on port 8080
