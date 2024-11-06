FROM alpine:3.20 as common
RUN apk add --no-cache libbz2 libmicrohttpd libsodium qt5-qtbase qt5-qtbase-sqlite qt5-qtwebsockets

## Build container
FROM common as builder
RUN apk add cargo cmake g++ libmicrohttpd-dev libsodium-dev make qt5-qtbase-dev qt5-qtwebsockets-dev

# On ARM, the C and C++ compilers have hiccups that make them randomy segfault.
# We just retry the compilation up to ten times to work around that.
ARG version=main
RUN mkdir -p /build \
    && cd /build \
    && wget -O /build/drawpile.zip https://github.com/drawpile/Drawpile/archive/${version}.zip \
    && unzip drawpile.zip \
    && cd Drawpile-*/ \
    && cmake --preset linux-release-qt5-server-make -DENABLE_ARCH_NATIVE=OFF -DINSTALL_DOC=OFF -DSOURCE_ASSETS=OFF \
    && while ! make -C build; do \
        tries=$((tries + 1)); \
        if [ "$tries" -gt 9 ]; then \
            echo "Compilation attempt $tries failed, giving up"; \
            exit 1; \
        else \
            echo "Compilation attempt $tries failed, trying again"; \
        fi; \
        echo 'Compilation failed, trying again'; \
    done  \
    && mv build/bin/drawpile-srv /build

# Final deployment image
FROM common
COPY --from=builder /build/drawpile-srv /bin
RUN adduser -D drawpile

EXPOSE 27750
USER drawpile
ENTRYPOINT ["/bin/drawpile-srv"]

