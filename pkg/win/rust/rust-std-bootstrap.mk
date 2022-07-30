# This file is part of MXE. See LICENSE.md for licensing information.

PKG                     := rust-std-bootstrap
$(PKG)_BASE             := $(subst -bootstrap,,$(PKG))
$(PKG)_WEBSITE          := https://www.rust-lang.org/
$(PKG)_VERSION          := 1.62.0
$(PKG)_DEPS_$(BUILD)    := cc

ifneq (, $(findstring darwin,$(BUILD)))
BUILD_TRIPLET = $(firstword $(call split,-,$(BUILD)))-apple-darwin
else
    ifneq (, $(findstring linux,$(BUILD)))
    BUILD_TRIPLET = $(firstword $(call split,-,$(BUILD)))-unknown-linux-gnu
    else
    BUILD_TRIPLET = $(BUILD)
    endif
endif

TARGET_TRIPLET          = $(firstword $(call split,-,$(TARGET)))-pc-windows-gnu
$(PKG)_FILE             := $($(PKG)_BASE)-$($(PKG)_VERSION)-$(BUILD_TRIPLET).tar.gz
$(PKG)_URL              := https://static.rust-lang.org/dist/$($(PKG)_FILE)

# CHECKSUMS
CHECKSUM_rust-std-1.62.0-x86_64-unknown-linux-gnu       :=  80c3a88c8a972d20b2ac59adc789495c508d6b70d1fad2aa12e419ab683638dc

$(PKG)_CHECKSUM         := $(CHECKSUM_$($(PKG)_BASE)-$($(PKG)_VERSION)-$(BUILD_TRIPLET))
$(PKG)_TARGETS          := $(BUILD)

define $(PKG)_UPDATE
    stable_date := $(shell curl https://static.rust-lang.org/dist/channel-rust-stable-date.txt)
    $(WGET) -q -O- 'https://static.rust-lang.org/dist/$(stable_date)/' | \
    $(SED) -n 's,.*$($(PKG)_BASE)-\([0-9][^>]*\)\.tar.*,\1,p' | \
    grep -v 'alpha\|beta\|rc\|git\|nightly' | \
    $(SORT) -Vr | \
    head -1
endef

define $(PKG)_BUILD
    $(SOURCE_DIR)/$($(PKG)_BASE)-$($(PKG)_VERSION)-$(BUILD_TRIPLET)/install.sh --prefix=$(PREFIX)/$(BUILD) --verbose
endef
