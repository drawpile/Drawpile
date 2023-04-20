// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TIMELINE_DOCK_H
#define TIMELINE_DOCK_H

#include "desktop/widgets/timelinewidget.h"
#include <QDockWidget>
#include <QModelIndex>

class QAction;
class QCheckBox;
class QSpinBox;

namespace canvas {
class TimelineModel;
}

namespace drawdance {
class Message;
}

namespace widgets {
class GroupedToolButton;
class TimelineWidget;
}

namespace docks {

class Timeline final : public QDockWidget {
	Q_OBJECT
public:
	Timeline(QWidget *parent);

	void setTimeline(canvas::TimelineModel *model);
	void setActions(const widgets::TimelineWidget::Actions &actions);

	void setFrameCount(int frameCount);
	void setCurrentLayer(int layerId);

	int currentFrame() const;

public slots:
	void setFeatureAccess(bool access);

signals:
	void timelineEditCommands(int count, const drawdance::Message *msgs);
	void frameSelected(int frame);
	void trackHidden(int trackId, bool hidden);
	void trackOnionSkinEnabled(int trackId, bool onionSkin);

private slots:
	void setCompatibilityMode(bool compatibilityMode);

private:
	bool isCompatibilityMode() const;

	void updateControlsEnabled(bool access, bool compatibilityMode);

	widgets::TimelineWidget *m_widget;
	bool m_featureAccessEnabled;
};

}

#endif
