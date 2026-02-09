// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_WIDGETS_TIMELAPSEPREVIEW
#define DESKTOP_WIDGETS_TIMELAPSEPREVIEW
#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QRect>
#include <QSize>
#include <QWidget>

struct DP_ViewModeFilter;

namespace drawdance {
class CanvasState;
}

namespace widgets {

class Spinner;

class TimelapsePreview final : public QWidget {
	Q_OBJECT
public:
	TimelapsePreview(QWidget *parent = nullptr);

	void setCanvas(
		const drawdance::CanvasState &canvasState,
		DP_ViewModeFilter *vmfOrNull);
	void setCropRect(const QRect &cropRect);
	void setOutputSize(const QSize &outputSize);
	void setLogoImage(const QImage &logoImage);
	void setLogoRect(const QRect &logoRect);
	void setLogoOpacity(double logoOpacity);
	void setAcceptRender(bool acceptRender);
	void setRenderedFrame(const QImage &renderedFrame);

protected:
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	void onCanvasToImageFinished(const QImage &img, unsigned int correlationId);
	void updateSpinner();
	bool updateRects();

	static QRect centerInRect(const QRect &parentRect, const QSize &childSize);

	Spinner *m_spinner = nullptr;
	QColor m_backgroundColor;
	QPixmap m_canvasImage;
	QPixmap m_canvasCache;
	QPixmap m_logoImage;
	QPixmap m_logoCache;
	QImage m_renderedFrame;
	QRect m_cropRect;
	QRect m_logoRect;
	double m_logoOpacity = 1.0;
	QSize m_canvasSize;
	QSize m_outputSize;
	QRect m_outputRect;
	QRect m_canvasRect;
	QRect m_scaledLogoRect;
	QRect m_renderedFrameRect;
	unsigned int m_canvasStateCorrelationId = 0;
	bool m_acceptRender = false;
};

}

#endif
