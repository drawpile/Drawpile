PKG             := extra-cmake-modules
$(PKG)_WEBSITE  := https://community.kde.org/Frameworks
$(PKG)_DESCR    := KDE Frameworks 5 Extra CMake Modules
$(PKG)_VERSION  := 5.54.0
$(PKG)_CHECKSUM := 91b7a9359f1bfe6f667a5a9c23f6b2178555df26ca2e4dd1bb5c38dc36c77144
$(PKG)_SUBDIR   := $(PKG)-$($(PKG)_VERSION)
$(PKG)_FILE     := $($(PKG)_SUBDIR).tar.xz
$(PKG)_URL      := http://download.kde.org/stable/frameworks/5.54/$($(PKG)_FILE)
$(PKG)_DEPS     := gcc qtbase

define $(PKG)_BUILD
	cd '$(1)' && '$(TARGET)-cmake' .
	$(MAKE) -C '$(1)' install
endef

