// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/canvas/selectionmodel.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/drawdance/selection.h"


namespace canvas {

SelectionModel::SelectionModel(QObject *parent)
	: QObject(parent)
{
}

void SelectionModel::setSelections(const drawdance::SelectionSet &ss)
{
	drawdance::Selection sel =
		ss.isNull() ? drawdance::Selection::null()
					: ss.search(m_localUserId, CanvasModel::MAIN_SELECTION_ID);
	if(sel.isNull()) {
		if(!m_content.isNull()) {
			m_content = drawdance::LayerContent::null();
			m_bounds = QRect();
			m_mask = QImage();
			emit selectionChanged(false, m_bounds, m_mask);
		}
	} else {
		drawdance::LayerContent content = sel.content();
		if(content.get() != m_content.get()) {
			m_content = content;
			m_bounds = sel.bounds();
			m_mask = m_content.toImageMask(m_bounds, QColor(0, 170, 255));
			emit selectionChanged(true, m_bounds, m_mask);
		}
	}
}

}
