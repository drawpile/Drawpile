/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2021 Calle Laakkonen

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

#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/floodfill.h"

#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/net/client.h"
#include "libclient/net/envelopebuilder.h"

#include <QGuiApplication>
#include <QPixmap>

namespace tools {

FloodFill::FloodFill(ToolController &owner)
	: Tool(owner, FLOODFILL, QCursor(QPixmap(":cursors/bucket.png"), 2, 29)),
	m_tolerance(0.01), m_expansion(0), m_sizelimit(1000*1000), m_sampleMerged(true), m_underFill(true),
	m_eraseMode(false)
{
}

void FloodFill::begin(const canvas::Point &point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	Q_UNUSED(right);
	const QColor color = owner.activeBrush().qColor();

	QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	net::EnvelopeBuilder writer;

	if(rustpile::paintengine_floodfill(
		owner.model()->paintEngine()->engine(),
		writer,
		owner.model()->localUserId(),
		owner.activeLayer(),
		point.x(),
		point.y(),
		rustpile::Color {
			static_cast<float>(color.redF()),
			static_cast<float>(color.greenF()),
			static_cast<float>(color.blueF()),
			m_eraseMode ? 0.0f : 1.0f,
		},
		m_tolerance,
		m_sampleMerged,
		m_sizelimit,
		m_expansion,
		m_underFill
	)) {
		owner.client()->sendEnvelope(writer.toEnvelope());
	}

	QGuiApplication::restoreOverrideCursor();
}

void FloodFill::motion(const canvas::Point &point, bool constrain, bool center)
{
	Q_UNUSED(point);
	Q_UNUSED(constrain);
	Q_UNUSED(center);
}

void FloodFill::end()
{
}

}
