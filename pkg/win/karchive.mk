PKG             := karchive
$(PKG)_WEBSITE  := https://community.kde.org/Frameworks
$(PKG)_DESCR    := KDE Frameworks 5 KArchive
$(PKG)_VERSION  := 5.31.0
$(PKG)_CHECKSUM := 5ee96cb2b0fa12fa6b0e6e38ffa70e5a5fc2bc9205ee08f5f041265cda9b4ef9
$(PKG)_SUBDIR   := $(PKG)-$($(PKG)_VERSION)
$(PKG)_FILE     := $($(PKG)_SUBDIR).tar.xz
$(PKG)_URL      := http://download.kde.org/stable/frameworks/5.31/$($(PKG)_FILE)
$(PKG)_DEPS     := gcc qtbase

define $(PKG)_BUILD
	mkdir '$(1)/build'
	cd '$(1)/build' && '$(TARGET)-cmake' .. -DTESTS=off
	$(MAKE) -C '$(1)/build' install
endef

