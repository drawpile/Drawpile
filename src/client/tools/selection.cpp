/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2015 Calle Laakkonen

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

#include "canvas/canvasmodel.h"
#include "net/client.h"

#include "tools/selection.h"
#include "tools/toolcontroller.h"
#include "tools/utils.h"

#include <QPixmap>
#include <QtMath>

namespace tools {

void SelectionTool::begin(const paintcore::Point &point, float zoom)
{
	canvas::Selection *sel = owner.model()->selection();
	if(sel)
		m_handle = sel->handleAt(point, zoom);
	else
		m_handle = canvas::Selection::OUTSIDE;

	m_start = point;
	m_p1 = point;

	if(m_handle == canvas::Selection::OUTSIDE) {
		if(sel) {
			sel->pasteToCanvas(owner.client(), owner.activeLayer());
			sel->setMovedFromCanvas(false);
		}

		sel = new canvas::Selection;
		initSelection(sel);
		owner.model()->setSelection(sel);
	}
}

void SelectionTool::motion(const paintcore::Point &point, bool constrain, bool center)
{
	canvas::Selection *sel = owner.model()->selection();
	if(!sel)
		return;

	if(m_handle==canvas::Selection::OUTSIDE) {
		newSelectionMotion(point, constrain, center);

	} else {
		QPointF p = point - m_start;

		if(sel->pasteImage().isNull() && !owner.model()->stateTracker()->isLayerLocked(owner.activeLayer())) {
			// Automatically cut the layer when the selection is transformed
			QImage img = owner.model()->selectionToImage(owner.activeLayer());
			sel->fillCanvas(Qt::white, paintcore::BlendMode::MODE_ERASE, owner.client(), owner.activeLayer());
			sel->setPasteImage(img);
			sel->setMovedFromCanvas(true);
		}

		if(m_handle == canvas::Selection::TRANSLATE && center) {
			// We use the center constraint during translation to rotate the selection
			const QPointF center = sel->boundingRect().center();

			double a0 = qAtan2(m_start.y() - center.y(), m_start.x() - center.x());
			double a1 = qAtan2(point.y() - center.y(), point.x() - center.x());

			sel->rotate(a1-a0);

		} else {
			sel->adjustGeometry(m_handle, p.toPoint(), constrain);
		}

		m_start = point;
	}
}

void SelectionTool::end()
{
	canvas::Selection *sel = owner.model()->selection();
	if(!sel)
		return;

	// Remove tiny selections
	QRectF selrect = sel->boundingRect();
	if(selrect.width() * selrect.height() <= 2)
		owner.model()->setSelection(nullptr);
}

RectangleSelection::RectangleSelection(ToolController &owner)
	: SelectionTool(owner, SELECTION, QCursor(QPixmap(":cursors/select-rectangle.png"), 2, 2))
{
}

void RectangleSelection::initSelection(canvas::Selection *selection)
{
	QPoint p = m_start.toPoint();
	selection->setShapeRect(QRect(p, p));
}

void RectangleSelection::newSelectionMotion(const paintcore::Point& point, bool constrain, bool center)
{
	QPointF p;
	if(constrain)
		p = constraints::square(m_start, point);
	else
		p = point;

	if(center)
		m_p1 = m_start - (p.toPoint() - m_start);
	else
		m_p1 = m_start;

	owner.model()->selection()->setShapeRect(QRectF(m_p1, p).normalized().toRect());
}

PolygonSelection::PolygonSelection(ToolController &owner)
	: SelectionTool(owner, POLYGONSELECTION, QCursor(QPixmap(":cursors/select-lasso.png"), 2, 2))
{
}

void PolygonSelection::initSelection(canvas::Selection *selection)
{
	selection->setShape(QPolygonF({ m_start }));
}

void PolygonSelection::newSelectionMotion(const paintcore::Point &point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);

	Q_ASSERT(owner.model()->selection());
	owner.model()->selection()->addPointToShape(point);
}

void PolygonSelection::end()
{
	if(owner.model()->selection())
		owner.model()->selection()->closeShape();

	SelectionTool::end();
}


}

