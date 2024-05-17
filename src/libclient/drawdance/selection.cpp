// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/geom.h>
#include <dpengine/selection.h>
}
#include "libclient/drawdance/selection.h"
#include <QPoint>

namespace drawdance {

Selection Selection::null()
{
	return Selection(nullptr);
}

Selection Selection::inc(DP_Selection *sel)
{
	return Selection(DP_selection_incref_nullable(sel));
}

Selection Selection::noinc(DP_Selection *sel)
{
	return Selection(sel);
}

Selection::Selection(const Selection &other)
	: Selection(DP_selection_incref_nullable(other.m_data))
{
}

Selection::Selection(Selection &&other)
	: Selection(other.m_data)
{
	other.m_data = nullptr;
}

Selection &Selection::operator=(const Selection &other)
{
	DP_selection_decref_nullable(m_data);
	m_data = DP_selection_incref_nullable(other.m_data);
	return *this;
}

Selection &Selection::operator=(Selection &&other)
{
	DP_selection_decref_nullable(m_data);
	m_data = other.m_data;
	other.m_data = nullptr;
	return *this;
}

Selection::~Selection()
{
	DP_selection_decref_nullable(m_data);
}

bool Selection::isNull() const
{
	return !m_data;
}

unsigned int Selection::contextId() const
{
	return DP_selection_context_id(m_data);
}

int Selection::selectionId() const
{
	return DP_selection_id(m_data);
}

QRect Selection::bounds() const
{
	DP_Rect r = *DP_selection_bounds(m_data);
	return QRect(QPoint(r.x1, r.y1), QPoint(r.x2, r.y2));
}

LayerContent Selection::content() const
{
	return LayerContent::inc(DP_selection_content_noinc(m_data));
}

Selection::Selection(DP_Selection *sel)
	: m_data(sel)
{
}

}
