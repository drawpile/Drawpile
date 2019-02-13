PKG             := kdnssd
$(PKG)_WEBSITE  := https://community.kde.org/Frameworks
$(PKG)_DESCR    := KDE Frameworks 5 KDNSSD
$(PKG)_VERSION  := 5.55.0
$(PKG)_CHECKSUM := ec9bf96ea760061d8b3a6efaf70f74de5554fe59ff8cfcdd04a6e2daeb919252
$(PKG)_SUBDIR   := $(PKG)-$($(PKG)_VERSION)
$(PKG)_FILE     := $($(PKG)_SUBDIR).tar.xz
$(PKG)_URL      := http://download.kde.org/stable/frameworks/5.55/$($(PKG)_FILE)
$(PKG)_DEPS     := cc qtbase dnssd_shim

define $(PKG)_BUILD
	mkdir '$(1)/build'
	cd '$(1)/build' && '$(TARGET)-cmake' .. -DBUILD_TESTING=off
	$(MAKE) -C '$(1)/build' install
endef

