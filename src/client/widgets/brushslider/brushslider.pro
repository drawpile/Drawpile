TEMPLATE = lib
CONFIG += designer plugin debug_and_release
target.path = $$[QT_INSTALL_PLUGINS]/designer
DEFINES += DESIGNER_PLUGIN
INSTALLS += target

# Input
RESOURCES = resources.qrc
HEADERS += ../../brushslider.h plugin.h
SOURCES += ../../brushslider.cpp plugin.cpp
