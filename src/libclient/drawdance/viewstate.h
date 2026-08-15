// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_DRAWDANCE_VIEWSTATE_H
#define LIBCLIENT_DRAWDANCE_VIEWSTATE_H
#include <QPointF>
#include <QSize>
#include <cmath>
#include <dpengine/view_state.h>

namespace drawdance {

class ViewState final {
public:
	ViewState() {}

	ViewState(
		QSize viewportSize, QPointF pos, qreal zoom, qreal rotation,
		bool mirror, bool flip)
		: m_viewState({
			  viewportSize.width(),
			  viewportSize.height(),
			  pos.x(),
			  pos.y(),
			  zoom,
			  normalizeRotation(rotation),
			  mirror,
			  flip,
		  })
	{
	}

	int viewportWidth() const { return m_viewState.viewport_width; }
	int viewportHeight() const { return m_viewState.viewport_height; }
	qreal x() const { return m_viewState.x; }
	qreal y() const { return m_viewState.y; }
	qreal zoom() const { return m_viewState.zoom; }
	qreal rotation() const { return m_viewState.rotation; }
	bool mirror() const { return m_viewState.mirror; }
	bool flip() const { return m_viewState.flip; }

	QSize viewportSize() const
	{
		return QSize(viewportWidth(), viewportHeight());
	}

	QPointF pos() const { return QPointF(x(), y()); }

	DP_ViewState &viewState() { return m_viewState; }
	const DP_ViewState &constViewState() const { return m_viewState; }

	bool isValid() const { return DP_view_state_valid(m_viewState); }

	bool operator==(const ViewState &other) const
	{
		return this == &other ||
			   DP_view_state_equal(m_viewState, other.m_viewState);
	}

private:
	static qreal normalizeRotation(qreal rotation)
	{
		qreal n = std::fmod(rotation, 360.0);
		if(n >= 0.0) {
			return n;
		} else {
			return n + 360.0;
		}
	}

	DP_ViewState m_viewState = DP_VIEW_STATE_INVALID_INIT;
};

}

#endif
