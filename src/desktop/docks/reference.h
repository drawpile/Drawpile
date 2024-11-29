// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DOCKS_REFERENCE_H
#define DESKTOP_DOCKS_REFERENCE_H
#include "desktop/docks/dockbase.h"
#include <QPointF>

class QButtonGroup;
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

private:
	static constexpr int ACTION_PAN = 0;
	static constexpr int ACTION_PICK = 1;
	static constexpr int ZOOM_UNSET = 0;
	static constexpr int ZOOM_TO_FIT = 1;
	static constexpr int ZOOM_RESET = 2;

	void openReferenceFile();
	void showReferenceImage(const QImage &img);

	void setZoomFromSlider(int percent);
	void setZoomFromView(qreal zoom);
	void updateZoomButton();
	void zoomResetOrFit();

	widgets::ReferenceView *m_view;
	QButtonGroup *m_buttonGroup;
	KisSliderSpinBox *m_zoomSlider;
	widgets::GroupedToolButton *m_zoomButton;
	int m_zoomButtonMode = ZOOM_UNSET;
};

}

#endif
