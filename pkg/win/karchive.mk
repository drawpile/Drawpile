PKG             := karchive
$(PKG)_WEBSITE  := https://community.kde.org/Frameworks
$(PKG)_DESCR    := KDE Frameworks 5 KArchive
$(PKG)_VERSION  := 5.54.0
$(PKG)_CHECKSUM := 8f28ab8a8f7236ae5e9e6cf35263dbbb87a52ec938d35515f073bc33dbc33d90
$(PKG)_SUBDIR   := $(PKG)-$($(PKG)_VERSION)
$(PKG)_FILE     := $($(PKG)_SUBDIR).tar.xz
$(PKG)_URL      := http://download.kde.org/stable/frameworks/5.54/$($(PKG)_FILE)
$(PKG)_DEPS     := gcc qtbase

define $(PKG)_BUILD
	mkdir '$(1)/build'
	cd '$(1)/build' && '$(TARGET)-cmake' .. -DBUILD_TESTING=off
	$(MAKE) -C '$(1)/build' install
endef

