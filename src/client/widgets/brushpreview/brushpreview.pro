TEMPLATE = lib
CONFIG += designer plugin debug_and_release
target.path = $$[QT_INSTALL_PLUGINS]/designer
DEFINES += DESIGNER_PLUGIN
INSTALLS += target

# Input
RESOURCES = resources.qrc
HEADERS += ../../brushpreview.h plugin.h
SOURCES += ../../brushpreview.cpp ../../core/brush.cpp ../../core/layer.cpp ../../core/tile.cpp plugin.cpp
