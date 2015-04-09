/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#ifndef ANIMATION_H
#define ANIMATION_H

#include <QObject>
#include <QColor>

class QWidget;
class VideoExporter;

namespace paintcore {
	class LayerStack;
}

class AnimationExporter : public QObject
{
	Q_OBJECT
public:
	AnimationExporter(paintcore::LayerStack *layers, VideoExporter *exporter, QObject *parent);

	static void exportAnimation(paintcore::LayerStack *layers, QWidget *parent);

	void start();

signals:
	void error(const QString &message);
	void progress(int frame);
	void done();

private slots:
	void saveNextFrame();

private:
	paintcore::LayerStack *_layers;
	VideoExporter *_exporter;

	int _startFrame, _endFrame;
	bool _useBgLayer;
	QColor _bgColor;

	int _currentFrame;
};


#endif // ANIMATION_H
