FROM dockcross/windows-x64

# Add MXE dependencies and customized settings.mk
WORKDIR /usr/src/mxe
RUN git checkout master
RUN git pull
ADD settings.mk /usr/src/mxe/
RUN make -j$(nproc)

RUN make download-qt5
RUN make download-miniupnpc download-giflib download-libsodium

# Patch Qt
ADD qtbase-2-no-tabletevent.patch /usr/src/mxe/src/

# Build MXE dependencies
RUN make -j$(nproc) qt5 miniupnpc giflib libsodium

# Add our own deps
ADD extra-cmake-modules.mk karchive.mk /usr/src/mxe/src/
RUN make download-karchive download-extra-cmake-modules
ADD karchive-1-notests.patch /usr/src/mxe/src/

# Build our dependencies
RUN make extra-cmake-modules karchive

