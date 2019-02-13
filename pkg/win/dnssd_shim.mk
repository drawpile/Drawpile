PKG             := dnssd_shim
$(PKG)_WEBSITE  := https://github.com/callaa/dnssd_shim
$(PKG)_DESCR    := dnssd.dll dynamic loader shim
$(PKG)_VERSION  := 3abe0ee
$(PKG)_CHECKSUM := aaa58801ff3fcc541db06dfdede3537c9c33346bbb7a3d82912dc7069653cd32
$(PKG)_GH_CONF  := callaa/dnssd_shim/branches/master
$(PKG)_DEPS     := cc

define $(PKG)_BUILD
        CC='$(PREFIX)/bin/$(TARGET)-gcc' AR='$(PREFIX)/bin/$(TARGET)-ar' $(MAKE) -C '$(1)'
        cp '$(1)/libdnssd_shim.a' '$(PREFIX)/$(TARGET)/lib'
        cp '$(1)/include/dns_sd.h' '$(PREFIX)/$(TARGET)/include'
endef

