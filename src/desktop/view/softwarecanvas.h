// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_VIEW_SOFTWARECANVAS_H
#define DESKTOP_VIEW_SOFTWARECANVAS_H
#include "desktop/view/canvasinterface.h"
#include <QList>
#include <QLoggingCategory>
#include <QRectF>
#include <QWidget>

namespace view {

class CanvasController;

class SoftwareCanvas final : public QWidget, public CanvasInterface {
	Q_OBJECT
public:
	SoftwareCanvas(CanvasController *controller, QWidget *parent = nullptr);
	~SoftwareCanvas() override;

	QWidget *asWidget() override;

	QSize viewSize() const override;
	QPointF viewToCanvasOffset() const override;
	QPointF viewTransformOffset() const override;

	void handleResize(QResizeEvent *event) override;
	void handlePaint(QPaintEvent *event) override;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	void setCheckerColor1(const QColor &checkerColor1);
	void setCheckerColor2(const QColor &checkerColor2);
	void setRenderUpdateFull(bool renderUpdateFull);
	void onControllerTransformChanged();
	void onControllerOutlineChanged();
	void onControllerTileCacheDirtyCheckNeeded();
	void onSceneChanged(const QList<QRectF> &region);

	struct Private;
	Private *d;
};

}

#endif
