TEMPLATE = lib

CONFIG = warn_on staticlib

TARGET = dpshared

*:release { DEFINES += NDEBUG }

HEADERS = protocol.h sockets.h
SOURCES = protocol.cpp sockets.cpp

