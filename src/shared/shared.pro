TEMPLATE = lib

CONFIG = warn_on staticlib

TARGET = dpshared

*:release { DEFINES += NDEBUG }

HEADERS = protocol.h sockets.h SHA1.h
SOURCES = protocol.cpp sockets.cpp SHA1.cpp

HEADERS += protocol.admin.h protocol.defaults.h protocol.errors.h
HEADERS += protocol.tools.h protocol.flags.h protocol.types.h
