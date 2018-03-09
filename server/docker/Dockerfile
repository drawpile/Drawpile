FROM ubuntu:18.04
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
    libmicrohttpd-dev \
    libsodium-dev \
    -y \
 && rm -rf /var/lib/apt/lists/* /var/cache/apt/archives/*

ARG VERSION=master
RUN git clone --branch $VERSION --single-branch --depth 1 https://github.com/drawpile/Drawpile.git \
 && cd Drawpile \
 && mkdir build \
 && cd build \
 && cmake .. -DCLIENT=off -DSERVERGUI=off \
 && make \
 && make install

RUN cd / && rm -rf /Drawpile
RUN useradd -m drawpile
USER drawpile
EXPOSE 8080
ENTRYPOINT ["drawpile-srv", "--listen", "0.0.0.0", "--port", "8080"]
