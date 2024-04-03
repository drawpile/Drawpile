// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/view/canvaswrapper.h"
#include "desktop/scene/scenewrapper.h"
#include "desktop/view/viewwrapper.h"
#include "libclient/settings.h"

namespace view {

CanvasWrapper *
CanvasWrapper::instantiate(int canvasImplementation, QWidget *parent)
{
	using libclient::settings::CanvasImplementation;
	switch(canvasImplementation) {
	case int(CanvasImplementation::OpenGl):
		return new ViewWrapper(true, parent);
	case int(CanvasImplementation::Software):
		return new ViewWrapper(false, parent);
	case int(CanvasImplementation::GraphicsView):
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
