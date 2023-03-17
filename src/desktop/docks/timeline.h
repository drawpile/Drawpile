/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2021 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
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
