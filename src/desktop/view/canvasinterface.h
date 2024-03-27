// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_VIEW_CANVASINTERFACE_H
#define DESKTOP_VIEW_CANVASINTERFACE_H
#include <QSize>

class QPaintEvent;
class QResizeEvent;
class QWidget;

namespace view {

class CanvasController;

class CanvasInterface {
public:
	virtual ~CanvasInterface() = default;

	virtual QWidget *asWidget() = 0;

	virtual QSize viewSize() const = 0;

	virtual void handleResize(QResizeEvent *event) = 0;
	virtual void handlePaint(QPaintEvent *event) = 0;
};

}

#endif
