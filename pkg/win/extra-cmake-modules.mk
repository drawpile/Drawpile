PKG             := extra-cmake-modules
$(PKG)_WEBSITE  := https://community.kde.org/Frameworks
$(PKG)_DESCR    := KDE Frameworks 5 Extra CMake Modules
$(PKG)_VERSION  := 5.55.0
$(PKG)_CHECKSUM := 649453922aef38a24af04258ab6661ddfd566aaba4d1773a9e1f4799344406f5
$(PKG)_SUBDIR   := $(PKG)-$($(PKG)_VERSION)
$(PKG)_FILE     := $($(PKG)_SUBDIR).tar.xz
$(PKG)_URL      := http://download.kde.org/stable/frameworks/5.55/$($(PKG)_FILE)
$(PKG)_DEPS     := gcc qtbase

define $(PKG)_BUILD
	cd '$(1)' && '$(TARGET)-cmake' .
	$(MAKE) -C '$(1)' install
endef

