TEMPLATE = lib
CONFIG += designer plugin debug_and_release
target.path = $$[QT_INSTALL_PLUGINS]/designer
DEFINES += DESIGNER_PLUGIN
INSTALLS += target

# Input
HEADERS += ../../qtcolortriangle.h plugin.h
SOURCES += ../../qtcolortriangle.cpp plugin.cpp
