## Common base
FROM alpine:3.9 as common
RUN apk add --no-cache qt5-qtbase qt5-qtbase-sqlite libmicrohttpd libbz2 libsodium

## Build container
FROM common as builder
RUN apk add qt5-qtbase-dev libmicrohttpd-dev libsodium-dev cmake make g++
WORKDIR /build/

COPY build-deps.sh /build/
RUN sh build-deps.sh

ARG version=master
RUN wget https://github.com/drawpile/Drawpile/archive/${version}.zip -O /build/drawpile.zip
COPY build.sh /build/
RUN sh build.sh

## Final deployment image
FROM common
COPY --from=builder /build/drawpile-srv /bin
COPY --from=builder /build/karchive*/build/bin/libKF5* /usr/lib/
RUN adduser -D drawpile

EXPOSE 27750
USER drawpile
ENTRYPOINT ["/bin/drawpile-srv"]

