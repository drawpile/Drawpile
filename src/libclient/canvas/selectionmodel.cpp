// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/threading.h>
}
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/selectionmodel.h"
#include "libclient/drawdance/selection.h"

namespace canvas {

SelectionMask::SelectionMask(
	SelectionModel *model, const drawdance::LayerContent &content,
	const QRect &bounds)
	: m_model(model)
	, m_content(content)
	, m_bounds(bounds)
{
}

const QImage &SelectionMask::image() const
{
	if(!m_maskValid) {
		if(m_model) {
			m_model->reifyImageMask(m_bounds, m_content, m_image, m_maskValid);
		} else {
			// Should only happen during teardown, so whatever.
			m_maskValid = true;
		}
	}
	return m_image;
}


SelectionModel::SelectionModel(QObject *parent)
	: QObject(parent)
	, m_mutex(DP_mutex_new())
{
}

SelectionModel::~SelectionModel()
{
	DP_mutex_free(m_mutex);
}

void SelectionModel::reifyImageMask(
	const QRect &bounds, const drawdance::LayerContent &content,
	QImage &outMask, bool &inOutMaskValid)
{
	DP_MUTEX_MUST_LOCK(m_mutex);
	if(!inOutMaskValid) {
		inOutMaskValid = true;
		outMask = content.toImageMask(bounds);
	}
	DP_MUTEX_MUST_UNLOCK(m_mutex);
}

void SelectionModel::setSelections(const drawdance::SelectionSet &ss)
{
	drawdance::Selection sel =
		ss.isNull() ? drawdance::Selection::null()
					: ss.search(m_localUserId, CanvasModel::MAIN_SELECTION_ID);
	if(sel.isNull()) {
		if(m_mask) {
			m_mask.clear();
			emit selectionChanged(m_mask);
		}
	} else {
		drawdance::LayerContent content = sel.content();
		if(!m_mask || m_mask->content().get() != content.get()) {
			m_mask.reset(new SelectionMask(this, content, sel.bounds()));
			emit selectionChanged(m_mask);
		}
	}
}

}
