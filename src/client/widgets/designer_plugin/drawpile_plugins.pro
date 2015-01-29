TEMPLATE = lib
CONFIG += plugin release
QT += widgets designer concurrent
target.path = $$[QT_INSTALL_PLUGINS]/designer
DEFINES += DESIGNER_PLUGIN
INSTALLS += target
QMAKE_CXXFLAGS += -std=c++11
INCLUDEPATH += ../../

# Input
#RESOURCES = resources.qrc

HEADERS += collection.h \
	../colorbutton.h colorbutton_plugin.h \
	../brushpreview.h brushpreview_plugin.h ../../core/layerstack.h

SOURCES += collection.cpp \
	../colorbutton.cpp colorbutton_plugin.cpp \
	../brushpreview.cpp brushpreview_plugin.cpp ../../core/brush.cpp ../../core/brushmask.cpp ../../core/layer.cpp ../../core/layerstack.cpp ../../core/tile.cpp ../../core/rasterop.cpp ../../core/shapes.cpp ../../core/floodfill.cpp
