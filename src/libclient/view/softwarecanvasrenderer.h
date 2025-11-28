// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_VIEW_SOFTWARECANVASRENDERER_H
#define LIBCLIENT_VIEW_SOFTWARECANVASRENDERER_H
#include <QBrush>
#include <QColor>

class QPainter;
class QRect;

namespace view {

class CanvasControllerBase;

class SoftwareCanvasRenderer {
public:
	void setCheckerColor1(const QColor &checkerColor1);
	void setCheckerColor2(const QColor &checkerColor2);

	void paint(
		CanvasControllerBase *controller, QPainter *painter, const QRect &rect);

private:
	const QBrush &getCheckerBrush();
	static bool isCheckersVisible(CanvasControllerBase *controller);

	QColor m_checkerColor1;
	QColor m_checkerColor2;
	QBrush m_checkerBrush;
};

}

#endif
