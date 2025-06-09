// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_LASSOFILL_H
#define LIBCLIENT_TOOLS_LASSOFILL_H
#include "libclient/drawdance/brushengine.h"
#include "libclient/tools/clickdetector.h"
#include "libclient/tools/tool.h"
#include <QImage>
#include <QPointF>
#include <QTimer>

namespace tools {

class LassoFillTool final : public Tool {
public:
	LassoFillTool(ToolController &owner);

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void end(const EndParams &params) override;

	void finishMultipart() override;
	void cancelMultipart() override;
	void undoMultipart() override;
	bool isMultipart() const override;

	void setSelectionMaskingEnabled(bool selectionMaskingEnabled) override;

	void setParams(
		float opacity, int stabilizationMode, int stabilizerSampleCount,
		int smoothing, int blendMode, bool antiAlias);

private:
	class Shape {
	public:
		void begin(
			bool antiAlias, int layerId, int blendMode, const QColor &color,
			const QRect &selBounds, const QImage &selImage);
		void clear();

		bool isPending() const { return m_pending; }

		int pointCount() const;
		void addPoint(const QPointF &point);

		int layerId() const { return m_layerId; }
		int blendMode() const { return m_blendMode; }

		bool get(QPoint *outPos, const QImage **outImage);

	private:
		void updateImage();

		bool m_pending = false;
		bool m_antiAlias = false;
		bool m_imageValid = false;
		int m_layerId = 0;
		int m_blendMode = 0;
		QColor m_color;
		QPolygon m_polygon;
		QPolygonF m_polygonF;
		QPoint m_pos;
		QRect m_selBounds;
		QImage m_selImage;
		QImage m_image;
	};

	int getEffectiveStabilizerSampleCount() const;
	int getEffectiveSmoothing() const;

	void addPoint(const QPointF &point);
	void pollControl(bool enable);
	void poll();

	void requestPendingPreviewUpdate();
	void updatePendingPreview();

	QTimer m_pollTimer;
	drawdance::StrokeEngine m_strokeEngine;
	long long m_lastTimeMsec = 0LL;
	float m_opacity = 1.0f;
	int m_stabilizationMode = 0;
	int m_stabilizerSampleCount = 0;
	int m_smoothing = 0;
	int m_blendMode;
	bool m_antiAlias = false;
	bool m_drawing = false;
	bool m_previewUpdatePending = false;
	ClickDetector m_clickDetector;
	Shape m_shape;
};

}

#endif
