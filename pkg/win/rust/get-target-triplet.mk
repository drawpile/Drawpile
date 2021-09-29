# This file is part of MXE. See LICENSE.md for licensing information.

PKG                     := get-target-triplet
$(PKG)_VERSION          := 1.0.0
$(PKG)_TYPE             := meta
$(PKG)_DEPS             := cc

define $(PKG)_BUILD
    printf $(TARGET) > $(MXE_TMP)/get-target-triplet.txt
endef
