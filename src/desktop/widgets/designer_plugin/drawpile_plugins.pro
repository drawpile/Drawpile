TEMPLATE = lib
CONFIG += plugin release
QT += widgets designer
target.path = $$[QT_INSTALL_PLUGINS]/designer
DEFINES += DESIGNER_PLUGIN
INSTALLS += target
QMAKE_CXXFLAGS += -std=c++11
INCLUDEPATH += ../../
INCLUDEPATH += ../../../client/

# Input
#RESOURCES = resources.qrc

HEADERS += collection.h \
	../colorbutton.h colorbutton_plugin.h \
	../groupedtoolbutton.h groupedtoolbutton_plugin.h \
	../filmstrip.h filmstrip_plugin.h \
	../resizerwidget.h resizer_plugin.h \
	../brushpreview.h brushpreview_plugin.h ../../../client/core/layerstack.h ../../../client/core/annotationmodel.h \
	../tablettest.h tablettester_plugin.h

SOURCES += collection.cpp \
	../colorbutton.cpp colorbutton_plugin.cpp \
	../groupedtoolbutton.cpp groupedtoolbutton_plugin.cpp \
	../filmstrip.cpp filmstrip_plugin.cpp \
	../resizerwidget.cpp resizer_plugin.cpp \
	../brushpreview.cpp brushpreview_plugin.cpp ../../../client/core/brush.cpp ../../../client/core/brushmask.cpp ../../../client/core/layer.cpp ../../../client/core/layerstack.cpp ../../../client/core/tile.cpp ../../../client/core/rasterop.cpp ../../../client/core/shapes.cpp ../../../client/core/floodfill.cpp ../../../client/core/annotationmodel.cpp \
	../tablettest.cpp tablettester_plugin.cpp

