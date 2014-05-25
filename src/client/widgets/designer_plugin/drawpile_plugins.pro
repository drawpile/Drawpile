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
	../brushpreview.h brushpreview_plugin.h ../../core/layerstack.h \
	../dualcolorbutton.h dualcolorbutton_plugin.h \
	../imageselector.h imageselector_plugin.h

SOURCES += collection.cpp \
	../colorbutton.cpp colorbutton_plugin.cpp \
	../brushpreview.cpp brushpreview_plugin.cpp ../../core/brush.cpp ../../core/brushmask.cpp ../../core/layer.cpp ../../core/layerstack.cpp ../../core/tile.cpp ../../core/rasterop.cpp ../../core/shapes.cpp \
	../dualcolorbutton.cpp dualcolorbutton_plugin.cpp \
	../imageselector.cpp imageselector_plugin.cpp
