TEMPLATE = app

CONFIG = warn_on

TARGET = dpsrv
LIBS += -L../shared/ -ldpshared

LIBS += -lws2_32
#win32:LIBS += -lws2_32

HEADERS = event.h
SOURCES = dpsrv.cpp event.cpp
