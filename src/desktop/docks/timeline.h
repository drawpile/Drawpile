// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TIMELINE_DOCK_H
#define TIMELINE_DOCK_H

#include <QDockWidget>
#include <QModelIndex>

class QCheckBox;
class QSpinBox;

namespace canvas {
	class TimelineModel;
}

namespace drawdance {
	class Message;
}

namespace widgets {
    class TimelineWidget;
}

namespace docks {

class Timeline final : public QDockWidget {
	Q_OBJECT
public:
	Timeline(QWidget *parent);

	void setTimeline(canvas::TimelineModel *model);

	void setFps(int fps);
	void setUseTimeline(bool useTimeline);

	int currentFrame() const;
	void setCurrentFrame(int frame, int layerId);

public slots:
	void setNextFrame();
	void setCurrentLayer(int layerId);
	void setPreviousFrame();
	void setFeatureAccess(bool access);

signals:
	void timelineEditCommands(int count, const drawdance::Message *msgs);
	void currentFrameChanged(int frame);
	void layerSelectRequested(int layerId);

private slots:
	void setCompatibilityMode(bool compatibilityMode);
	void onFrameChanged(int frame);
	void onUseTimelineClicked();
	void onFpsChanged();
	void onFramesChanged();
	void autoSelectLayer();

private:
	widgets::TimelineWidget *m_widget;
	QCheckBox *m_useTimeline;
	QSpinBox *m_currentFrame;
	QSpinBox *m_fps;
};

}

#endif
