PKG             := extra-cmake-modules
$(PKG)_WEBSITE  := https://community.kde.org/Frameworks
$(PKG)_DESCR    := KDE Frameworks 5 Extra CMake Modules
$(PKG)_VERSION  := 5.31.0
$(PKG)_CHECKSUM := 84271a59045468ced64823f030d3dc9f157cec16a30387483ec888639fdc2deb
$(PKG)_SUBDIR   := $(PKG)-$($(PKG)_VERSION)
$(PKG)_FILE     := $($(PKG)_SUBDIR).tar.xz
$(PKG)_URL      := http://download.kde.org/stable/frameworks/5.31/$($(PKG)_FILE)
$(PKG)_DEPS     := gcc qtbase

define $(PKG)_BUILD
	cd '$(1)' && '$(TARGET)-cmake' .
	$(MAKE) -C '$(1)' install
endef

