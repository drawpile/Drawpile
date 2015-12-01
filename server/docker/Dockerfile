FROM ubuntu:15.04
RUN apt-get update \
 && apt-get install \
    wget \
    cmake \
    make \
    qtbase5-dev \
    libkf5archive-dev \
    g++ \
    qtm \
    qtmultimedia5-dev \
    libqt5svg5-dev \
    libgif-dev \
    libkf5dnssd-dev \
    -y \
 && rm -rf /var/lib/apt/lists/* /var/cache/apt/archives/*

ENV VERSION 1.0.2

RUN wget http://drawpile.net/files/src/drawpile-$VERSION.tar.gz \
 && tar -xf drawpile-$VERSION.tar.gz \
 && cd drawpile-$VERSION \
 && mkdir build \
 && cd build \
 && cmake .. \
 && make \
 && make install

RUN cd / && rm -rf /drawpile-*
RUN useradd -m drawpile
USER drawpile
EXPOSE 8080
CMD drawpile-srv --persistent --listen 0.0.0.0 --port 8080 --verbose --title "Docker Drawpile"
