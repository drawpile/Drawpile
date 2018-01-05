PKG             := karchive
$(PKG)_WEBSITE  := https://community.kde.org/Frameworks
$(PKG)_DESCR    := KDE Frameworks 5 KArchive
$(PKG)_VERSION  := 5.41.0
$(PKG)_CHECKSUM := 43c40f06e8a5e3198e5363a82748b3a7cb79526489e6bb651ca59e509297a741
$(PKG)_SUBDIR   := $(PKG)-$($(PKG)_VERSION)
$(PKG)_FILE     := $($(PKG)_SUBDIR).tar.xz
$(PKG)_URL      := http://download.kde.org/stable/frameworks/5.41/$($(PKG)_FILE)
$(PKG)_DEPS     := gcc qtbase

define $(PKG)_BUILD
	mkdir '$(1)/build'
	cd '$(1)/build' && '$(TARGET)-cmake' .. -DTESTS=off
	$(MAKE) -C '$(1)/build' install
endef

