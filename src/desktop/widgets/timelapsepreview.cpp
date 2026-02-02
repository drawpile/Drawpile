// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/timelapsepreview.h"
#include "desktop/widgets/spinner.h"
#include "libclient/drawdance/canvasstate.h"
#include "libclient/utils/canvastoimagerunnable.h"
#include <QBrush>
#include <QLoggingCategory>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QThreadPool>
#include <QtMath>

Q_LOGGING_CATEGORY(
	lcDpTimelapsePreview, "net.drawpile.widgets.timelapsepreview", QtWarningMsg)

namespace widgets {

TimelapsePreview::TimelapsePreview(QWidget *parent)
	: QWidget(parent)
{
}

void TimelapsePreview::setCanvas(const drawdance::CanvasState &canvasState)
{
	m_canvasImage = QPixmap();
	m_canvasSize = canvasState.isNull() ? QSize() : canvasState.size();
	m_backgroundColor =
		canvasState.isNull()
			? QColor()
			: canvasState.backgroundTile().singleColor(QColor());
	++m_canvasStateCorrelationId;

	if(m_canvasSize.isEmpty()) {
		qCDebug(
			lcDpTimelapsePreview, "Canvas %u is empty",
			m_canvasStateCorrelationId);

	} else {
		qCDebug(
			lcDpTimelapsePreview, "Canvas %u at %dx%d",
			m_canvasStateCorrelationId, m_canvasSize.width(),
			m_canvasSize.height());

		if(!m_spinner) {
			m_spinner = new Spinner(this);
			updateSpinner();
		}

		CanvasToImageRunnable *canvasToImageRunnable =
			new CanvasToImageRunnable(canvasState, m_canvasStateCorrelationId);
		connect(
			canvasToImageRunnable, &CanvasToImageRunnable::finished, this,
			&TimelapsePreview::onCanvasToImageFinished, Qt::QueuedConnection);
		QThreadPool::globalInstance()->start(canvasToImageRunnable);
	}

	updateRects();
	update();
}

void TimelapsePreview::setCropRect(const QRect &cropRect)
{
	if(cropRect != m_cropRect) {
		m_cropRect = cropRect;
		m_canvasCache = QPixmap();
		updateRects();
		update();
	}
}

void TimelapsePreview::setOutputSize(const QSize &outputSize)
{
	m_outputSize = outputSize;
	if(updateRects()) {
		update();
	}
}

void TimelapsePreview::setLogoImage(const QImage &logoImage)
{
	m_logoImage = QPixmap::fromImage(logoImage);
	m_logoCache = QPixmap();
	if(!m_logoRect.isEmpty()) {
		update();
	}
}

void TimelapsePreview::setLogoRect(const QRect &logoRect)
{
	if(logoRect != m_logoRect) {
		m_logoRect = logoRect;
		m_logoCache = QPixmap();
		if(updateRects()) {
			update();
		}
	}
}

void TimelapsePreview::setLogoOpacity(double logoOpacity)
{
	if(logoOpacity != m_logoOpacity) {
		m_logoOpacity = logoOpacity;
		update();
	}
}

void TimelapsePreview::setAcceptRender(bool acceptRender)
{
	if(acceptRender != m_acceptRender) {
		m_acceptRender = acceptRender;
		if(!acceptRender && !m_renderedFrame.isNull()) {
			m_renderedFrame = QImage();
			updateRects();
			update();
		}
	}
}

void TimelapsePreview::setRenderedFrame(const QImage &renderedFrame)
{
	if(m_acceptRender) {
		bool sizeChanged = renderedFrame.size() != m_renderedFrame.size();
		m_renderedFrame = renderedFrame;
		if(sizeChanged) {
			updateRects();
		}
		update();
	}
}

void TimelapsePreview::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	m_canvasCache = QPixmap();
	if(m_spinner) {
		updateSpinner();
	}
	updateRects();
}

void TimelapsePreview::paintEvent(QPaintEvent *event)
{
	Q_UNUSED(event);
	QRect widgetRect = rect();
	if(!widgetRect.isEmpty()) {
		QPainter painter(this);
		QPalette pal = palette();

		painter.fillRect(widgetRect, pal.alternateBase());

		if(!m_renderedFrameRect.isEmpty()) {
			painter.drawImage(m_renderedFrameRect, m_renderedFrame);
		} else if(!m_outputRect.isEmpty()) {
			if(!m_spinner && m_backgroundColor.isValid()) {
				painter.fillRect(m_outputRect, m_backgroundColor);
			} else {
				painter.fillRect(m_outputRect, pal.base());
			}

			qreal dpr = devicePixelRatioF();

			if(!m_spinner && !m_canvasImage.isNull() &&
			   !m_canvasRect.isEmpty()) {
				QSize cacheSize(
					qRound(qreal(m_canvasRect.width()) * dpr),
					qRound(qreal(m_canvasRect.height()) * dpr));
				if(cacheSize.isEmpty()) {
					m_canvasCache = QPixmap();
				} else {
					if(m_canvasCache.size() != cacheSize) {
						if(m_cropRect.isEmpty()) {
							m_canvasCache = m_canvasImage.scaled(
								cacheSize, Qt::IgnoreAspectRatio,
								Qt::SmoothTransformation);
						} else {
							m_canvasCache =
								m_canvasImage.copy(m_cropRect)
									.scaled(
										cacheSize, Qt::IgnoreAspectRatio,
										Qt::SmoothTransformation);
						}
					}
					painter.drawPixmap(m_canvasRect, m_canvasCache);
				}
			}

			if(!m_logoImage.isNull() && !m_scaledLogoRect.isEmpty() &&
			   m_logoOpacity > 0.0) {
				QSize cacheSize(
					qRound(qreal(m_scaledLogoRect.width()) * dpr),
					qRound(qreal(m_scaledLogoRect.height()) * dpr));
				if(cacheSize.isEmpty()) {
					m_logoCache = QPixmap();
				} else {
					if(m_logoCache.size() != cacheSize) {
						m_logoCache = m_logoImage.scaled(
							cacheSize, Qt::IgnoreAspectRatio,
							Qt::SmoothTransformation);
					}
					painter.setOpacity(m_logoOpacity);
					painter.drawPixmap(m_scaledLogoRect, m_logoCache);
				}
			}
		}
	}
}

void TimelapsePreview::onCanvasToImageFinished(
	const QImage &img, unsigned int correlationId)
{
	qCDebug(
		lcDpTimelapsePreview, "Got %dx%d canvas image %u at correlation id %u",
		img.width(), img.height(), correlationId, m_canvasStateCorrelationId);

	if(correlationId == m_canvasStateCorrelationId && !img.isNull() &&
	   !img.size().isEmpty()) {
		delete m_spinner;
		m_spinner = nullptr;
		m_canvasImage = QPixmap::fromImage(img);
		m_canvasCache = QPixmap();
		update();
	}
}

void TimelapsePreview::updateSpinner()
{
	m_spinner->setGeometry(centerInRect(rect(), QSize(width(), height() / 2)));
}

bool TimelapsePreview::updateRects()
{
	QRect outputRect;
	QRect canvasRect;
	QRect scaledLogoRect;
	QRect renderedFrameRect;

	QRect widgetRect = rect();
	if(!widgetRect.isEmpty() && !m_outputSize.isEmpty()) {
		outputRect = centerInRect(widgetRect, m_outputSize);

		if(!m_canvasSize.isEmpty()) {
			if(m_cropRect.isEmpty()) {
				canvasRect = centerInRect(outputRect, m_canvasSize);
			} else {
				canvasRect = centerInRect(outputRect, m_cropRect.size());
			}
		}

		if(!m_logoRect.isEmpty()) {
			qreal scale =
				((qreal(outputRect.width()) / qreal(m_outputSize.width())) +
				 (qreal(outputRect.height()) / qreal(m_outputSize.height()))) /
				2.0;
			scaledLogoRect = QRect(
				outputRect.x() + qRound(qreal(m_logoRect.x()) * scale),
				outputRect.y() + qRound(qreal(m_logoRect.y()) * scale),
				qRound(qreal(m_logoRect.width()) * scale),
				qRound(qreal(m_logoRect.height()) * scale));
		}

		if(!m_renderedFrame.isNull()) {
			renderedFrameRect =
				centerInRect(widgetRect, m_renderedFrame.size());
		}
	}

	if(outputRect != m_outputRect || canvasRect != m_canvasRect ||
	   scaledLogoRect != m_scaledLogoRect ||
	   renderedFrameRect != m_renderedFrameRect) {
		m_outputRect = outputRect;
		m_canvasRect = canvasRect;
		m_scaledLogoRect = scaledLogoRect;
		m_renderedFrameRect = renderedFrameRect;
		return true;
	} else {
		return false;
	}
}

QRect TimelapsePreview::centerInRect(
	const QRect &parentRect, const QSize &childSize)
{
	QSizeF resultSize =
		QSizeF(childSize).scaled(parentRect.size(), Qt::KeepAspectRatio);
	QRect resultRect = QRect(
		0, 0, qBound(1, qCeil(resultSize.width() + 0.5), parentRect.width()),
		qBound(1, qCeil(resultSize.height() + 0.5), parentRect.height()));
	resultRect.moveCenter(parentRect.center());
	return resultRect;
}

}
