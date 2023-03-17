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

namespace drawdance {
    class Message;
}

namespace widgets {

class TimelineWidget final : public QWidget
{
    Q_OBJECT
public:
	explicit TimelineWidget(QWidget *parent = nullptr);
	~TimelineWidget() override;

	void setModel(canvas::TimelineModel *model);
	void setCurrentFrame(int frame);
	void setCurrentLayer(int layerId);
	void setEditable(bool editable);

	canvas::TimelineModel *model() const;

	int currentFrame() const;
	int currentLayerId() const;

signals:
	void timelineEditCommands(int count, const drawdance::Message *msgs);
	void selectFrameRequest(int frame, int layerId);

protected:
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void wheelEvent(QWheelEvent *event) override;

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
