// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/view/canvaswrapper.h"
#include "desktop/scene/scenewrapper.h"
#include "desktop/view/viewwrapper.h"
#include "libclient/view/enums.h"

namespace view {

CanvasWrapper *
CanvasWrapper::instantiate(int canvasImplementation, QWidget *parent)
{
	switch(canvasImplementation) {
	case int(view::CanvasImplementation::OpenGl):
		return new ViewWrapper(true, parent);
	case int(view::CanvasImplementation::Software):
		return new ViewWrapper(false, parent);
	case int(view::CanvasImplementation::GraphicsView):
		return new drawingboard::SceneWrapper(parent);
	default:
		qWarning(
			"Unknown canvas implementation %d, using Qt Graphics Scene",
			canvasImplementation);
		return new drawingboard::SceneWrapper(parent);
	}
}

CanvasWrapper::~CanvasWrapper() {}

}
