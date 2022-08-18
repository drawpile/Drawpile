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

namespace net {
	class Envelope;
}

namespace canvas {
	class TimelineModel;
}

namespace widgets {
    class TimelineWidget;
}

namespace docks {

class Timeline : public QDockWidget {
	Q_OBJECT
public:
	Timeline(QWidget *parent);

	void setTimeline(canvas::TimelineModel *model);

	void setFps(int fps);
	void setUseTimeline(bool useTimeline);

	int currentFrame() const;
	void setCurrentFrame(int frame);

public slots:
	void setNextFrame();
	void setPreviousFrame();
	void setFeatureAccess(bool access);

signals:
	void timelineEditCommand(const net::Envelope &e);
	void currentFrameChanged(int frame);

private slots:
	void onUseTimelineClicked();
	void onFpsChanged();
	void onFramesChanged();

private:
	widgets::TimelineWidget *m_widget;
	QCheckBox *m_useTimeline;
	QSpinBox *m_currentFrame;
	QSpinBox *m_fps;
};

}

#endif
