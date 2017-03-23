TEMPLATE = lib
CONFIG += plugin release
QT += widgets designer
target.path = $$[QT_INSTALL_PLUGINS]/designer
DEFINES += DESIGNER_PLUGIN
INSTALLS += target
QMAKE_CXXFLAGS += -std=c++11
INCLUDEPATH += ../../

# Input
#RESOURCES = resources.qrc

HEADERS += collection.h \
	../colorbutton.h colorbutton_plugin.h \
	../groupedtoolbutton.h groupedtoolbutton_plugin.h \
	../filmstrip.h filmstrip_plugin.h \
	../resizerwidget.h resizer_plugin.h \
	../brushpreview.h brushpreview_plugin.h ../../core/layerstack.h ../../core/annotationmodel.h

SOURCES += collection.cpp \
	../colorbutton.cpp colorbutton_plugin.cpp \
	../groupedtoolbutton.cpp groupedtoolbutton_plugin.cpp \
	../filmstrip.cpp filmstrip_plugin.cpp \
	../resizerwidget.cpp resizer_plugin.cpp \
	../brushpreview.cpp brushpreview_plugin.cpp ../../core/brush.cpp ../../core/brushmask.cpp ../../core/layer.cpp ../../core/layerstack.cpp ../../core/tile.cpp ../../core/rasterop.cpp ../../core/shapes.cpp ../../core/floodfill.cpp ../../core/annotationmodel.cpp
