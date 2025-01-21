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
	../filmstrip.h filmstrip_plugin.h \
	../tablettest.h tablettester_plugin.h \
	../spinner.h spinner_plugin.h \
	../presetselector.h presetselector_plugin.h \
	../modifierkeys.h modifierkeys_plugin.h

SOURCES += collection.cpp \
	../colorbutton.cpp colorbutton_plugin.cpp \
	../filmstrip.cpp filmstrip_plugin.cpp \
	../tablettest.cpp tablettester_plugin.cpp \
	../spinner.cpp spinner_plugin.cpp \
	../presetselector.cpp presetselector_plugin.cpp \
	../modifierkeys.cpp modifierkeys_plugin.cpp \
	../../../libshared/util/paths.cpp

