TEMPLATE = lib
CONFIG += designer plugin debug_and_release
target.path = $$[QT_INSTALL_PLUGINS]/designer
DEFINES += NO_WIDGETS_NAMESPACE
INSTALLS += target

# Input
HEADERS += ../../colortriangle.h plugin.h
SOURCES += ../../colortriangle.cpp plugin.cpp
