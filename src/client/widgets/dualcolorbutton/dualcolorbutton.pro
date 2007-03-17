TEMPLATE = lib
CONFIG += designer plugin debug_and_release
target.path = $$[QT_INSTALL_PLUGINS]/designer
DEFINES += DESIGNER_PLUGIN
INSTALLS += target

# Input
RESOURCES = resources.qrc
HEADERS += ../../dualcolorbutton.h plugin.h
SOURCES += ../../dualcolorbutton.cpp plugin.cpp
