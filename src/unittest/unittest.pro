TEMPLATE = app

CONFIG = warn_on

TARGET = unittest

*:release { DEFINES += NDEBUG }

HEADERS = testing.h test.sha1.h test.memcpy_t.h test.bswap.h
SOURCES = unittest.cpp 

HEADERS += ../shared/templates.h ../shared/SHA1.cpp
SOURCES += ../shared/SHA1.cpp
