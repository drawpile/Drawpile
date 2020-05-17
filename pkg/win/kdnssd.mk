PKG             := kdnssd
$(PKG)_WEBSITE  := https://community.kde.org/Frameworks
$(PKG)_DESCR    := KDE Frameworks 5 KDNSSD
$(PKG)_VERSION  := 5.70.0
$(PKG)_CHECKSUM := fede0519a8d82bf1bc49cd486ec6c80e7f3cc42efa63dbc5c3591ce2ac9d4d71
$(PKG)_SUBDIR   := $(PKG)-$($(PKG)_VERSION)
$(PKG)_FILE     := $($(PKG)_SUBDIR).tar.xz
$(PKG)_URL      := http://download.kde.org/stable/frameworks/5.70/$($(PKG)_FILE)
$(PKG)_DEPS     := cc qtbase dnssd_shim

define $(PKG)_BUILD
	mkdir '$(1)/build'
	cd '$(1)/build' && '$(TARGET)-cmake' .. -DBUILD_TESTING=off
	$(MAKE) -C '$(1)/build' install
endef

