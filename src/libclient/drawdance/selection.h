// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_DRAWDANCE_SELECTION_H
#define LIBCLIENT_DRAWDANCE_SELECTION_H
#include "libclient/drawdance/layercontent.h"
#include <QRect>

struct DP_Selection;

namespace drawdance {

class Selection final {
public:
	static Selection null();
	static Selection inc(DP_Selection *sel);
	static Selection noinc(DP_Selection *sel);

	Selection(const Selection &other);
	Selection(Selection &&other);

	Selection &operator=(const Selection &other);
	Selection &operator=(Selection &&other);

	~Selection();

	bool isNull() const;

	unsigned int contextId() const;

	int selectionId() const;

	QRect calculateBounds() const;

	LayerContent content() const;

private:
	explicit Selection(DP_Selection *sel);

	DP_Selection *m_data;
};

}

#endif
