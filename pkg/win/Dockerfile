FROM dockcross/windows-x64

# Add MXE dependencies and customized settings.mk
WORKDIR /usr/src/mxe
RUN git checkout master
ADD settings.mk /usr/src/mxe/
RUN make -j$(nproc)

RUN make download-qt5
RUN make download-miniupnpc download-giflib

# Build MXE dependencies
RUN make -j$(nproc) qt5 miniupnpc giflib

# Add our own deps
ADD extra-cmake-modules.mk karchive.mk /usr/src/mxe/src/
RUN make download-karchive download-extra-cmake-modules
ADD karchive-1-notests.patch /usr/src/mxe/src/

# Build our dependencies
RUN make extra-cmake-modules karchive

