#include <QtQml>

#include "canvasmodel.h"
#include "lasertrailmodel.h"
#include "annotationstate.h"

void registerCanvasTypes() {
	qmlRegisterUncreatableType<canvas::CanvasModel>("Drawpile.Canvas", 1, 0, "CanvasModel", "This is for connecting with C++ code");
	qmlRegisterUncreatableType<canvas::LaserTrailModel>("Drawpile.Canvas", 1, 0, "LaserTrailModel", "This is for connecting with C++ code");
	qRegisterMetaType<canvas::Annotation>("Annotation");
}
