TEMPLATE = app

CONFIG = warn_on

TARGET = dpsrv
LIBS += -L../shared/ -ldpshared

win32* {
	LIBS += -lws2_32
}

*:release { DEFINES += NDEBUG }

HEADERS = event.h
SOURCES = dpsrv.cpp event.cpp
