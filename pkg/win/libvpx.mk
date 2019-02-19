# This file is part of MXE. See LICENSE.md for licensing information.
# Customized: disable unused features

PKG             := libvpx
$(PKG)_WEBSITE  := https://www.webmproject.org/code/
$(PKG)_DESCR    := vpx
$(PKG)_IGNORE   :=
$(PKG)_VERSION  := 1.5.0
$(PKG)_CHECKSUM := 306d67908625675f8e188d37a81fbfafdf5068b09d9aa52702b6fbe601c76797
$(PKG)_SUBDIR   := $(PKG)-$($(PKG)_VERSION)
$(PKG)_FILE     := $(PKG)-$($(PKG)_VERSION).tar.bz2
$(PKG)_URL      := https://storage.googleapis.com/downloads.webmproject.org/releases/webm/$($(PKG)_FILE)
$(PKG)_DEPS     := cc pthreads yasm

define $(PKG)_UPDATE
    $(WGET) -q -O- 'https://storage.googleapis.com/downloads.webmproject.org/releases/webm/index.html' | \
    $(SED) -n 's,.*libvpx-\([0-9][^>]*\)\.tar.*,\1,p' | \
    $(SORT) -Vr | \
    head -1
endef

define $(PKG)_BUILD
    $(SED) -i 's,yasm[ $$],$(TARGET)-yasm ,g' '$(1)/build/make/configure.sh'
    cd '$(1)' && \
        CROSS='$(TARGET)-' \
        ./configure \
        --prefix='$(PREFIX)/$(TARGET)' \
        --target=@libvpx-target@ \
        --disable-examples \
        --disable-install-docs \
        --disable-vp8 --disable-vp9-decoder --disable-multithread \
        --as=$(TARGET)-yasm \
        --extra-cflags='-std=gnu89'
    $(MAKE) -C '$(1)' -j '$(JOBS)'
    $(MAKE) -C '$(1)' -j 1 install
    $(TARGET)-ranlib $(PREFIX)/$(TARGET)/lib/libvpx.a
endef

$(PKG)_BUILD_i686-w64-mingw32   = $(subst @libvpx-target@,x86-win32-gcc,$($(PKG)_BUILD))
$(PKG)_BUILD_x86_64-w64-mingw32 = $(subst @libvpx-target@,x86_64-win64-gcc,$($(PKG)_BUILD))
