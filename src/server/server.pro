TEMPLATE = app

CONFIG = warn_on
TARGET = dpsrv
LIBS += -L../shared/ -ldpshared -lws2_32

HEADERS =
SOURCES = dpsrv.cpp
