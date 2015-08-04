#include "layerstackitem.h"
#include "canvasinputarea.h"
#include "polylineitem.h"
#include "canvastransform.h"
#include "pressure.h"

void registerQuickTypes() {
	qmlRegisterType<LayerStackItem>("Drawpile.Canvas", 1, 0, "LayerStack");
	qmlRegisterType<CanvasInputArea>("Drawpile.Canvas", 1, 0, "CanvasInputArea");
	qmlRegisterType<CanvasTransform>("Drawpile.Canvas", 1, 0, "CanvasTransform");
	qmlRegisterType<PolyLineItem>("Drawpile.Canvas", 1, 0, "PolyLine");
	qRegisterMetaType<PressureMapping>("PressureMapping");
}
