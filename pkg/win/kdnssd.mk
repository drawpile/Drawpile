PKG             := kdnssd
$(PKG)_WEBSITE  := https://community.kde.org/Frameworks
$(PKG)_DESCR    := KDE Frameworks 5 KDNSSD
$(PKG)_VERSION  := 5.54.0
$(PKG)_CHECKSUM := 677ed3d572706cc896c1e05bb2f1fb1a6c50ffaa13d1c62de13e35eca1e85803
$(PKG)_SUBDIR   := $(PKG)-$($(PKG)_VERSION)
$(PKG)_FILE     := $($(PKG)_SUBDIR).tar.xz
$(PKG)_URL      := http://download.kde.org/stable/frameworks/5.54/$($(PKG)_FILE)
$(PKG)_DEPS     := cc qtbase dnssd_shim

define $(PKG)_BUILD
	mkdir '$(1)/build'
	cd '$(1)/build' && '$(TARGET)-cmake' .. -DBUILD_TESTING=off
	$(MAKE) -C '$(1)/build' install
endef

