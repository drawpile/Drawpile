// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/selection_set.h>
}
#include "libclient/drawdance/selectionset.h"

namespace drawdance {

SelectionSet SelectionSet::null()
{
	return SelectionSet(nullptr);
}

SelectionSet SelectionSet::inc(DP_SelectionSet *ss)
{
	return SelectionSet(DP_selection_set_incref_nullable(ss));
}

SelectionSet SelectionSet::noinc(DP_SelectionSet *ss)
{
	return SelectionSet(ss);
}

SelectionSet::SelectionSet(const SelectionSet &other)
	: SelectionSet(DP_selection_set_incref_nullable(other.m_data))
{
}

SelectionSet::SelectionSet(SelectionSet &&other)
	: SelectionSet(other.m_data)
{
	other.m_data = nullptr;
}

SelectionSet &SelectionSet::operator=(const SelectionSet &other)
{
	DP_selection_set_decref_nullable(m_data);
	m_data = DP_selection_set_incref_nullable(other.m_data);
	return *this;
}

SelectionSet &SelectionSet::operator=(SelectionSet &&other)
{
	DP_selection_set_decref_nullable(m_data);
	m_data = other.m_data;
	other.m_data = nullptr;
	return *this;
}

SelectionSet::~SelectionSet()
{
	DP_selection_set_decref_nullable(m_data);
}

bool SelectionSet::isNull() const
{
	return !m_data;
}

int SelectionSet::count() const
{
	return DP_selection_set_count(m_data);
}

Selection SelectionSet::search(unsigned int contextId, int selectionId) const
{
	return Selection::inc(
		DP_selection_set_search_noinc(m_data, contextId, selectionId));
}

SelectionSet::SelectionSet(DP_SelectionSet *ss)
	: m_data(ss)
{
}

}
