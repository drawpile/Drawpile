TEMPLATE = app

CONFIG = warn_on

TARGET = dpsrv
LIBS += -L../shared/ -ldpshared

win32* {
	LIBS += -lws2_32
}

*:release { DEFINES += NDEBUG }

HEADERS = server.h event.h
SOURCES = dpsrv.cpp server.cpp event.cpp
