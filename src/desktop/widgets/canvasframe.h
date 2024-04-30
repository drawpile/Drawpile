// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_CANVASFRAME_H
#define DESKTOP_WIDGETS_CANVASFRAME_H
#include <QWidget>

class QAbstractScrollArea;
class QPolygonF;

namespace widgets {

class RulerWidget;

class CanvasFrame final : public QWidget {
	Q_OBJECT
public:
	explicit CanvasFrame(
		QAbstractScrollArea *canvasView, QWidget *parent = nullptr);

	void setShowRulers(bool showRulers);
	void setTransform(qreal zoom, qreal angle);
	void setView(const QPolygonF &view);

private:
	static constexpr int RULER_WIDTH = 18;

    static qreal transformScale(qreal min, qreal max, int viewWidth);

	QAbstractScrollArea *m_canvasView;
	widgets::RulerWidget *m_hRuler;
	widgets::RulerWidget *m_vRuler;
};

}

#endif
