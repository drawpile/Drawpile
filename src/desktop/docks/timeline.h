// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TIMELINE_DOCK_H
#define TIMELINE_DOCK_H
#include "desktop/docks/dockbase.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/timelinewidget.h"
#include "libclient/utils/debouncetimer.h"

class QAction;
class QSpinBox;

namespace canvas {
class CanvasModel;
}

namespace net {
class Message;
}

namespace widgets {
class GroupedToolButton;
class TimelineWidget;
}

namespace docks {

class TitleWidget;

class Timeline final : public DockBase {
	Q_OBJECT
public:
	Timeline(QWidget *parent);

	void setCanvas(canvas::CanvasModel *canvas);

	void setActions(
		const widgets::TimelineWidget::Actions &actions,
		QAction *layerViewNormal, QAction *layerViewCurrentFrame,
		QAction *showFlipbook);

	int currentTrackId() const;
	int currentFrame() const;

	void updateKeyFrameColorMenuIcon();

public slots:
	void setFramerate(int framerate);
	void setFrameCount(int frameCount);
	void setCurrentLayer(int layerId);
	void setFeatureAccess(bool access);

signals:
	void timelineEditCommands(int count, const net::Message *msgs);
	void trackSelected(int frame);
	void frameSelected(int frame);
	void layerSelected(int layerId);
	void trackHidden(int trackId, bool hidden);
	void trackOnionSkinEnabled(int trackId, bool onionSkin);

private slots:
	void setCurrentFrame(int frame);
	void setLocked(bool locked);

private:
	void setUpTitleWidget(
		const widgets::TimelineWidget::Actions &actions,
		QAction *layerViewNormal, QAction *layerViewCurrentFrame,
		QAction *showFlipbook);

	static void addTitleButton(
		docks::TitleWidget *titlebar, QAction *action,
		widgets::GroupedToolButton::GroupPosition position);

	void updateControlsEnabled(bool access, bool locked);

	void updateFrame(int frame);

	widgets::TimelineWidget *m_widget;
	QSpinBox *m_frameSpinner;
	QSpinBox *m_framerateSpinner;
	DebounceTimer m_framerateDebounce;
	bool m_featureAccessEnabled;
	bool m_locked;
};

}

#endif
