FROM dockcross/windows-x64

WORKDIR /usr/src/mxe

# Get the latest MXE version that provides Qt 5.11
# KIS_TABLET code doesn not work correctly with Qt 5.12
RUN git fetch && git checkout 730aeba502031f64c119959fded5bab471e5f5e5

# Custom MXE settings
ADD settings.mk /usr/src/mxe/
RUN make -j$(nproc)

# Download MXE deps
RUN make download-qt5
RUN make download-miniupnpc download-giflib download-libsodium download-libvpx

# Patch Qt
ADD qtbase-2-no-tabletevent.patch /usr/src/mxe/src/

# Customized MXE dependencies
ADD libvpx.mk /usr/src/mxe/src/

# Build MXE dependencies
RUN make -j$(nproc) qt5 miniupnpc giflib libsodium libvpx

# Add our own deps
ADD extra-cmake-modules.mk karchive.mk dnssd_shim.mk kdnssd.mk kdnssd-1-qtendian.patch kdnssd-2-shim.patch /usr/src/mxe/src/
RUN make download-karchive download-extra-cmake-modules download-dnssd_shim download-kdnssd

# Build our dependencies
RUN make extra-cmake-modules karchive kdnssd

