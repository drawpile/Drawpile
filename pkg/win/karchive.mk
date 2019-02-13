PKG             := karchive
$(PKG)_WEBSITE  := https://community.kde.org/Frameworks
$(PKG)_DESCR    := KDE Frameworks 5 KArchive
$(PKG)_VERSION  := 5.55.0
$(PKG)_CHECKSUM := 8475efa46cdc054d9fb6336e42c6075fb037921a9147d4e5aa564a5e58b79fd2
$(PKG)_SUBDIR   := $(PKG)-$($(PKG)_VERSION)
$(PKG)_FILE     := $($(PKG)_SUBDIR).tar.xz
$(PKG)_URL      := http://download.kde.org/stable/frameworks/5.55/$($(PKG)_FILE)
$(PKG)_DEPS     := gcc qtbase

define $(PKG)_BUILD
	mkdir '$(1)/build'
	cd '$(1)/build' && '$(TARGET)-cmake' .. -DBUILD_TESTING=off
	$(MAKE) -C '$(1)/build' install
endef

