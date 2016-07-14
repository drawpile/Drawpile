/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2016 Calle Laakkonen

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

#include "animation.h"
#include "videoexporter.h"
#include "core/layerstack.h"
#include "core/layer.h"

AnimationExporter::AnimationExporter(paintcore::LayerStack *layers, VideoExporter *exporter, QObject *parent)
	: QObject(parent), m_layers(layers), m_exporter(exporter),
	  m_startFrame(0), m_endFrame(layers->layerCount()),
	  m_useBgLayer(false), m_bgColor(Qt::white),
	  m_currentFrame(0)
{
	connect(m_exporter, &VideoExporter::exporterReady, this, &AnimationExporter::saveNextFrame, Qt::QueuedConnection);
	connect(m_exporter, &VideoExporter::exporterError, this, &AnimationExporter::error);
	connect(m_exporter, &VideoExporter::exporterFinished, this, &AnimationExporter::done);

}

void AnimationExporter::start()
{
	m_exporter->start();
}

void AnimationExporter::saveNextFrame()
{
	if(m_currentFrame > m_endFrame) {
		m_exporter->finish();

	} else {
		QImage image = m_layers->flatLayerImage(m_currentFrame - 1, m_useBgLayer, m_bgColor);

		m_exporter->saveFrame(image, 1);

		emit progress(m_currentFrame);
		m_currentFrame++;
	}
}

