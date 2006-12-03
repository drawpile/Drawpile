TEMPLATE = app

CONFIG = warn_on
TARGET = drawpileserver
LIBS += -L../shared/ -ldpshared -lws2_32

HEADERS =
SOURCES = dpsrv.cpp
