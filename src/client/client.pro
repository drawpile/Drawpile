TEMPLATE = app

CONFIG += qt warn_on debug
TARGET = drawpile
LIBS += -L../shared

RESOURCES = ui/resources.qrc
FORMS = ui/brushsettings.ui ui/colordialog.ui ui/about.ui ui/newdialog.ui
HEADERS = mainwindow.h \
	netstatus.h dualcolorbutton.h editorview.h \
	toolsettingswidget.h colordialog.h brushpreview.h colortriangle.h \
	board.h controller.h layer.h brush.h user.h tools.h toolsettings.h interfaces.h \
	boardeditor.h aboutdialog.h gradientslider.h newdialog.h point.h
SOURCES = main.cpp mainwindow.cpp \
	netstatus.cpp dualcolorbutton.cpp editorview.cpp \
	toolsettingswidget.cpp colordialog.cpp brushpreview.cpp colortriangle.cpp \
	board.cpp controller.cpp layer.cpp brush.cpp user.cpp tools.cpp toolsettings.cpp \
	boardeditor.cpp aboutdialog.cpp gradientslider.cpp newdialog.cpp

