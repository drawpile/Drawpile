// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DOCKS_REFERENCE_H
#define DESKTOP_DOCKS_REFERENCE_H
#include "desktop/docks/dockbase.h"
#include <QPointF>

class QAction;
class QButtonGroup;
class QColor;
class KisSliderSpinBox;

namespace widgets {
class GroupedToolButton;
class ReferenceView;
}

namespace docks {

class ReferenceDock final : public DockBase {
	Q_OBJECT
public:
	explicit ReferenceDock(QWidget *parent);

signals:
	void colorPicked(const QColor &color);

private:
	static constexpr int ZOOM_UNSET = 0;
	static constexpr int ZOOM_TO_FIT = 1;
	static constexpr int ZOOM_RESET = 2;

	void updateMenuActions();
	void openReferenceFile();
	void pasteReferenceImage();
	void showReferenceImageFromFile(const QImage &img, const QString &error);
	void showReferenceImage(const QImage &img);
	void handleImageDrop(const QImage &img);
	void handlePathDrop(const QString &path);
	void closeReferenceImage();

	void setInteractionMode(int interactionMode);

	void setZoomFromSlider(int percent);
	void setZoomFromView(qreal zoom);

	widgets::ReferenceView *m_view;
	QAction *m_pasteAction;
	QAction *m_closeAction;
	QButtonGroup *m_buttonGroup;
	widgets::GroupedToolButton *m_resetZoomButton;
	widgets::GroupedToolButton *m_zoomToFitButton;
	KisSliderSpinBox *m_zoomSlider;
};

}

#endif
