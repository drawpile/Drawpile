// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_VIEW_GLCANVAS_H
#define DESKTOP_VIEW_GLCANVAS_H
#include "desktop/view/canvasinterface.h"
#include <QLoggingCategory>
#include <QOpenGLWidget>

namespace view {

class CanvasController;

class GlCanvas final : public QOpenGLWidget, public CanvasInterface {
	Q_OBJECT
public:
	GlCanvas(CanvasController *controller, QWidget *parent = nullptr);
	~GlCanvas() override;

	QWidget *asWidget() override;

	QSize viewSize() const override;
	QPointF viewToCanvasOffset() const override;
	QPointF viewTransformOffset() const override;

	void handleResize(QResizeEvent *event) override;
	void handlePaint(QPaintEvent *event) override;

	static const QStringList &getSystemInfo();

protected:
	void initializeGL() override;
	void paintGL() override;
	void resizeGL(int w, int h) override;

private:
	void setCheckerColor1(const QColor &checkerColor1);
	void setCheckerColor2(const QColor &checkerColor2);
	void setShouldUseMipmaps(bool useMipmaps);
	void onControllerRenderSmoothChanged();
	void onControllerCanvasSizeChanged();
	void onControllerTransformChanged();
	void onControllerTileCacheDirtyCheckNeeded();

	static QStringList systemInfo;

	struct Private;
	Private *d;
};

}

#endif
