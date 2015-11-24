# Docker Drawpile

[Docker](https://www.docker.com/) is a way to easily package applications into shippable 'containers'. This directory 
contains the Dockerfile defintion for creating a drawpile server. 

To build, simply run the following command in this directory:

```
docker build -t drawpile .
```

Which will download and compile the latest Drawpile server. At the end of this process you will have a Docker container image ready to be run.

To run the your Drawpile docker container, simply use the following command:

```
docker run --detach --tty --name drawpile -p 8080:8080 drawpile
``` 

This will start a drawpile server in the background running on port 8080. 
