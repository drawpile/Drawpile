PKG             := extra-cmake-modules
$(PKG)_WEBSITE  := https://community.kde.org/Frameworks
$(PKG)_DESCR    := KDE Frameworks 5 Extra CMake Modules
$(PKG)_VERSION  := 5.70.0
$(PKG)_CHECKSUM := 830da8d84cc737e024ac90d6ed767d10f9e21531e5f576a1660d4ca88bee8581
$(PKG)_SUBDIR   := $(PKG)-$($(PKG)_VERSION)
$(PKG)_FILE     := $($(PKG)_SUBDIR).tar.xz
$(PKG)_URL      := http://download.kde.org/stable/frameworks/5.70/$($(PKG)_FILE)
$(PKG)_DEPS     := gcc qtbase

define $(PKG)_BUILD
	cd '$(1)' && '$(TARGET)-cmake' .
	$(MAKE) -C '$(1)' install
endef

