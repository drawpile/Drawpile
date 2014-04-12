TEMPLATE = lib
CONFIG += plugin release
QT += widgets designer
target.path = $$[QT_INSTALL_PLUGINS]/designer
DEFINES += DESIGNER_PLUGIN
INSTALLS += target

# Input
HEADERS += ../qtcolortriangle.h plugin.h
SOURCES += ../qtcolortriangle.cpp plugin.cpp
