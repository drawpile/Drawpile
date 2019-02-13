FROM dockcross/windows-x64

WORKDIR /usr/src/mxe

# Get specific MXE version that is known to work (2019-01-10)
# Provides Qt 5.12.0
RUN git fetch && git checkout 62329b26b94cf1eeb37a24f73228eda2078e44ea

# Custom MXE settings
ADD settings.mk /usr/src/mxe/
RUN make -j$(nproc)

# Download MXE deps
RUN make download-qt5
RUN make download-miniupnpc download-giflib download-libsodium download-libvpx

# Patch Qt
ADD qtbase-2-no-tabletevent.patch /usr/src/mxe/src/

# Build MXE dependencies
RUN make -j$(nproc) qt5 miniupnpc giflib libsodium libvpx

# Add our own deps
ADD extra-cmake-modules.mk karchive.mk dnssd_shim.mk kdnssd.mk kdnssd-1-qtendian.patch kdnssd-2-shim.patch /usr/src/mxe/src/
RUN make download-karchive download-extra-cmake-modules download-dnssd_shim download-kdnssd

# Build our dependencies
RUN make extra-cmake-modules karchive kdnssd

