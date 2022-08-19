/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2022 Calle Laakkonen

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

#ifndef TIMELINEWIDGET_H
#define TIMELINEWIDGET_H

#include <QWidget>

namespace canvas {
    class TimelineModel;
}

namespace net {
    class Envelope;
}

namespace widgets {

class TimelineWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget *parent = nullptr);
	~TimelineWidget();

	void setModel(canvas::TimelineModel *model);
	void setCurrentFrame(int frame);
	void setCurrentLayer(int layerId);
	void setEditable(bool editable);

	canvas::TimelineModel *model() const;

	int currentFrame() const;
	int currentLayerId() const;

signals:
	void timelineEditCommand(const net::Envelope &e);
	void selectFrameRequest(int frame, int layerId);

protected:
	void paintEvent(QPaintEvent *);
	void mousePressEvent(QMouseEvent *event);
	void mouseDoubleClickEvent(QMouseEvent *event);
	void resizeEvent(QResizeEvent *event);
	void wheelEvent(QWheelEvent *event);

private slots:
	void setHorizontalScroll(int pos);
	void setVerticalScroll(int pos);

	void onLayersChanged();

private:
	void updateScrollbars();

	struct Private;
	Private *d;
};

}

#endif // TIMELINEWIDGET_H
