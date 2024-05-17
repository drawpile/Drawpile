// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_DRAWDANCE_SELECTIONSET_H
#define LIBCLIENT_DRAWDANCE_SELECTIONSET_H
#include "libclient/drawdance/selection.h"

struct DP_SelectionSet;

namespace drawdance {

class SelectionSet final {
public:
	static SelectionSet null();
	static SelectionSet inc(DP_SelectionSet *ss);
	static SelectionSet noinc(DP_SelectionSet *ss);

	SelectionSet(const SelectionSet &other);
	SelectionSet(SelectionSet &&other);

	SelectionSet &operator=(const SelectionSet &other);
	SelectionSet &operator=(SelectionSet &&other);

	~SelectionSet();

	bool isNull() const;

	int count() const;

	Selection search(unsigned int contextId, int selectionId) const;

private:
	explicit SelectionSet(DP_SelectionSet *ss);

	DP_SelectionSet *m_data;
};

}

#endif
