// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_GRADIENT_H
#define LIBCLIENT_TOOLS_GRADIENT_H
#include "libclient/tools/clickdetector.h"
#include "libclient/tools/tool.h"
#include "libclient/utils/debouncetimer.h"
#include <QAtomicInteger>
#include <QColor>
#include <QImage>
#include <QPointF>
#include <QVector>

class QGradient;

namespace tools {

class GradientTool final : public Tool {
public:
	static constexpr int HANDLE_RADIUS = 20;
	enum class Shape { Linear, Radial };
	enum class Spread { Pad, Repeat, Reflect };

	GradientTool(ToolController &owner);

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void hover(const HoverParams &params) override;
	void end(const EndParams &params) override;

	void finishMultipart() override;
	void cancelMultipart() override;
	void undoMultipart() override;
	void redoMultipart() override;
	bool isMultipart() const override;

	void setSelectionValid(bool selectionValid) override;

	void setParameters(
		const QColor &color1, const QColor &color2, Shape shape, Spread spread,
		qreal focus, int blendMode);

private:
	static constexpr int MAX_POINTS_STACK_DEPTH = 250;

	void pushPoints();
	bool updateHoverIndex(const QPointF &targetPoint);
	void updateAnchorLine();
	void updateCursor();

	void updatePending();
	QImage applyGradient(const QImage &mask, const QLineF &line) const;
	QImage applyLinearGradient(const QImage &mask, const QLineF &line) const;
	QImage applyRadialGradient(const QImage &mask, const QLineF &line) const;
	void prepareGradient(QGradient &gradient) const;
	QImage paintGradient(const QImage &mask, QGradient &gradient) const;

	void previewPending();

	QVector<QPointF> m_points;
	QVector<QPointF> m_originalPoints;
	QVector<QVector<QPointF>> m_pointsStack;
	QPointF m_dragStartPoint;
	qreal m_zoom = 1.0;
	qreal m_focus = 0.0;
	Shape m_shape = Shape::Linear;
	Spread m_spread = Spread::Pad;
	QColor m_color1 = Qt::black;
	QColor m_color2 = Qt::transparent;
	int m_blendMode;
	int m_pointsStackTop = -1;
	int m_hoverIndex = -1;
	int m_dragIndex = -1;
	bool m_dragging = false;
	bool m_hoverOutside = false;
	QPoint m_pendingPos;
	QImage m_pendingImage;
	DebounceTimer m_previewDebounce;
	ClickDetector m_clickDetector;
};

}

#endif
