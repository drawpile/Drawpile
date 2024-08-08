FROM alpine:3.19 as common
RUN apk add --no-cache libbz2 libmicrohttpd libsodium qt5-qtbase qt5-qtbase-sqlite qt5-qtwebsockets

## Build container
FROM common as builder
RUN apk add cargo cmake g++ libmicrohttpd-dev libsodium-dev qt5-qtbase-dev qt5-qtwebsockets-dev samurai

ARG version=main
RUN mkdir -p /build \
    && cd /build \
    && wget -O /build/drawpile.zip https://github.com/drawpile/Drawpile/archive/${version}.zip \
    && unzip drawpile.zip \
    && cd Drawpile-*/ \
    && cmake --preset linux-release-qt5-server-ninja -DENABLE_ARCH_NATIVE=OFF -DINSTALL_DOC=OFF -DSOURCE_ASSETS=OFF \
    && cmake --build build \
    && mv build/bin/drawpile-srv /build

# Final deployment image
FROM common
COPY --from=builder /build/drawpile-srv /bin
RUN adduser -D drawpile

EXPOSE 27750
USER drawpile
ENTRYPOINT ["/bin/drawpile-srv"]

