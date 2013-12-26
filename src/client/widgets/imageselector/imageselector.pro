TEMPLATE = lib
CONFIG += plugin release
QT += widgets designer
target.path = $$[QT_INSTALL_PLUGINS]/designer
DEFINES += DESIGNER_PLUGIN
INSTALLS += target
QMAKE_CXXFLAGS += -std=c++11

# Input
RESOURCES = resources.qrc
HEADERS += ../imageselector.h plugin.h
SOURCES += ../imageselector.cpp plugin.cpp
