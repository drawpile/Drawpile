TEMPLATE = app

CONFIG += qt warn_on
TARGET = drawpile
LIBS += -L../shared

RESOURCES = ui/resources.qrc
FORMS = 
HEADERS = mainwindow.h netstatus.h hostlabel.h editorview.h board.h controller.h layer.h brush.h user.h
SOURCES = main.cpp mainwindow.cpp netstatus.cpp hostlabel.cpp editorview.cpp board.cpp controller.cpp layer.cpp brush.cpp user.cpp

