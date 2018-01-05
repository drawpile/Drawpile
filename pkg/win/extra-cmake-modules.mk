PKG             := extra-cmake-modules
$(PKG)_WEBSITE  := https://community.kde.org/Frameworks
$(PKG)_DESCR    := KDE Frameworks 5 Extra CMake Modules
$(PKG)_VERSION  := 5.41.0
$(PKG)_CHECKSUM := baaf60940b9ff883332629ba2800090bb86722ba49a85cc12782e4ee5b169f67
$(PKG)_SUBDIR   := $(PKG)-$($(PKG)_VERSION)
$(PKG)_FILE     := $($(PKG)_SUBDIR).tar.xz
$(PKG)_URL      := http://download.kde.org/stable/frameworks/5.41/$($(PKG)_FILE)
$(PKG)_DEPS     := gcc qtbase

define $(PKG)_BUILD
	cd '$(1)' && '$(TARGET)-cmake' .
	$(MAKE) -C '$(1)' install
endef

