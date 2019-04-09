FROM dockcross/windows-x64

WORKDIR /usr/src/mxe

# Get specific MXE version that is known to work? (2019-03-28)
# Provides Qt 5.12.2
RUN git fetch && git checkout 46bfb30d596577537d6865b580e1ae520a74558c

# Custom MXE settings
ADD settings.mk /usr/src/mxe/
RUN make -j$(nproc)

# Download MXE deps
RUN make download-qt5
RUN make download-miniupnpc download-giflib download-libsodium download-libvpx

# Patch Qt
ADD qtbase-2-disable-wintab.patch qtbase-3-disable-winink.patch /usr/src/mxe/src/

# Customized MXE dependencies
ADD libvpx.mk /usr/src/mxe/src/

# Build MXE dependencies
RUN make -j$(nproc) qt5 miniupnpc giflib libsodium libvpx

# Add our own deps
ADD extra-cmake-modules.mk karchive.mk dnssd_shim.mk kdnssd.mk kdnssd-1-qtendian.patch kdnssd-2-shim.patch /usr/src/mxe/src/
RUN make download-karchive download-extra-cmake-modules download-dnssd_shim download-kdnssd

# Build our dependencies
RUN make extra-cmake-modules karchive kdnssd

# Create the app build directory
RUN mkdir /Build && chmod a+w /Build

