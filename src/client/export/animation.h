/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016 Calle Laakkonen

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

	void start();

	void setStartFrame(int f) {
		m_startFrame = f;
		if(m_currentFrame < f)
			m_currentFrame = f;
	}
	void setEndFrame(int f) { m_endFrame = f; }
	void setUseBgLayer(bool usebg) { m_useBgLayer = usebg; }
	void setBackground(const QColor &color) { m_bgColor = color; }

signals:
	void error(const QString &message);
	void progress(int frame);
	void done();

private slots:
	void saveNextFrame();

private:
	paintcore::LayerStack *m_layers;
	VideoExporter *m_exporter;

	int m_startFrame, m_endFrame;
	bool m_useBgLayer;
	QColor m_bgColor;

	int m_currentFrame;
};


#endif // ANIMATION_H
