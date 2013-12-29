TEMPLATE = lib
CONFIG += plugin_release
QT += widgets designer
target.path = $$[QT_INSTALL_PLUGINS]/designer
DEFINES += DESIGNER_PLUGIN
INSTALLS += target
QMAKE_CXXFLAGS += -std=c++11

# Input
RESOURCES = resources.qrc
HEADERS += ../gradientslider.h plugin.h
SOURCES += ../gradientslider.cpp plugin.cpp
