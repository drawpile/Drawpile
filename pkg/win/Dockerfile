FROM dockcross/windows-x64

WORKDIR /usr/src/mxe

# Get specific MXE version that is known to work? (2019-03-28)
# Provides Qt 5.12.2
RUN git fetch && git checkout 46bfb30d596577537d6865b580e1ae520a74558c

# Custom MXE settings
ADD settings.mk /usr/src/mxe/
RUN make -j$(nproc)

# Build Qt (takes a long time)
RUN make download-qt5
ADD qtbase-2-disable-wintab.patch qtbase-3-disable-winink.patch /usr/src/mxe/src/
RUN make -j$(nproc) qt5

# Patch and build other MXE dependencies
RUN make download-libsodium download-qtkeychain
RUN make -j$(nproc) libsodium qtkeychain

# Add our own deps
ADD extra-cmake-modules.mk karchive.mk /usr/src/mxe/src/
# This old Docker container seems to have issues with KDE mirror certificates.
# MXE checksums the downloaded files though, so we can just turn off certificate
# validation in wget without risking being given any malicious or whatever.
RUN perl -pi -e 's/^(WGET\s*=.*)/$1 --no-check-certificate/' Makefile
RUN make extra-cmake-modules karchive

# Create the app build directory
RUN mkdir /Build && chmod a+w /Build

