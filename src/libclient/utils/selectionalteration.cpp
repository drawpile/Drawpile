#include "libclient/utils/selectionalteration.h"

SelectionAlteration::SelectionAlteration(
	const drawdance::CanvasState &canvasState, unsigned int contextId,
	int selectionId, int expand, int kernel, int feather, bool fromEdge,
	QObject *parent)
	: QObject(parent)
	, m_canvasState(canvasState)
	, m_contextId(contextId)
	, m_selectionId(selectionId)
	, m_expand(expand)
	, m_kernel(kernel)
	, m_feather(feather)
	, m_fromEdge(fromEdge)
{
}

void SelectionAlteration::run()
{
	QImage img;
	int x, y;
	DP_FloodFillResult result = m_canvasState.selectionFill(
		m_contextId, m_selectionId, Qt::black, m_expand,
		DP_FloodFillKernel(m_kernel), m_feather, m_fromEdge, m_cancel, img, x,
		y);
	if(result == DP_FLOOD_FILL_SUCCESS) {
		emit success(img, x, y);
	} else {
		emit failure(int(result));
	}
}

void SelectionAlteration::cancel()
{
	m_cancel = true;
}
