// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TIMELINE_DOCK_H
#define TIMELINE_DOCK_H

#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/timelinewidget.h"
#include "libclient/utils/debouncetimer.h"
#include <QDockWidget>

class QAction;
class QSpinBox;

namespace canvas {
class CanvasModel;
}

namespace drawdance {
class Message;
}

namespace widgets {
class GroupedToolButton;
class TimelineWidget;
}

namespace docks {

class TitleWidget;

class Timeline final : public QDockWidget {
	Q_OBJECT
public:
	Timeline(QWidget *parent);

	void setCanvas(canvas::CanvasModel *canvas);

	void setActions(
		const widgets::TimelineWidget::Actions &actions,
		QAction *layerViewNormal, QAction *layerViewCurrentFrame);

	int currentFrame() const;

public slots:
	void setFramerate(int framerate);
	void setFrameCount(int frameCount);
	void setCurrentLayer(int layerId);
	void setFeatureAccess(bool access);

signals:
	void timelineEditCommands(int count, const drawdance::Message *msgs);
	void frameSelected(int frame);
	void layerSelected(int layerId);
	void trackHidden(int trackId, bool hidden);
	void trackOnionSkinEnabled(int trackId, bool onionSkin);

private slots:
	void setCurrentFrame(int frame);
	void setCompatibilityMode(bool compatibilityMode);

private:
	static constexpr int DEBOUNCE_DELAY_MS = 500;

	void setUpTitleWidget(
		const widgets::TimelineWidget::Actions &actions,
		QAction *layerViewNormal, QAction *layerViewCurrentFrame);

	static void addTitleButton(
		docks::TitleWidget *titlebar, QAction *action,
		widgets::GroupedToolButton::GroupPosition position);

	bool isCompatibilityMode() const;

	void updateControlsEnabled(bool access, bool compatibilityMode);
	void updateFrame(int frame);

	widgets::TimelineWidget *m_widget;
	QSpinBox *m_frameSpinner;
	QSpinBox *m_framerateSpinner;
	QSpinBox *m_frameCountSpinner;
	DebounceTimer m_framerateDebounce;
	DebounceTimer m_frameCountDebounce;
	bool m_featureAccessEnabled;
};

}

#endif
