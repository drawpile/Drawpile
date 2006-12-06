TEMPLATE = lib

CONFIG = warn_on
TARGET = dpshared

*:release { DEFINES += NDEBUG }

HEADERS = unittest.templates.h
SOURCES = unittest.cpp
