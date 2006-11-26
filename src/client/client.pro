TEMPLATE = app

CONFIG += qt warn_on
TARGET = drawpile
LIBS += -L../shared

RESOURCES = ui/resources.qrc
FORMS = ui/brushsettings.ui
HEADERS = mainwindow.h \
	netstatus.h hostlabel.h dualcolorbutton.h editorview.h \
	toolsettingswidget.h \
	board.h controller.h layer.h brush.h user.h tools.h toolsettings.h
SOURCES = main.cpp mainwindow.cpp \
	netstatus.cpp hostlabel.cpp dualcolorbutton.cpp editorview.cpp \
	toolsettingswidget.cpp \
	board.cpp controller.cpp layer.cpp brush.cpp user.cpp toolsettings.cpp

