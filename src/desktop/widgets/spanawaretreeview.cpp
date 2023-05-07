// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/widgets/spanawaretreeview.h"

#include <QDebug>
#include <QList>
#include <QModelIndex>

namespace widgets {

void SpanAwareTreeView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
	QTreeView::dataChanged(topLeft, bottomRight, roles);

	if (const auto *m = model()) {
		for (auto row = topLeft.row(); row <= bottomRight.row(); ++row) {
			setAllSpans(m->index(row, 0, topLeft.parent()));
		}
	}
}

void SpanAwareTreeView::rowsInserted(const QModelIndex &parent, int start, int end)
{
	QTreeView::rowsInserted(parent, start, end);

	if (const auto *m = model()) {
		for (auto row = start; row <= end; ++row) {
			setAllSpans(m->index(row, 0, parent));
		}
	}
}

void SpanAwareTreeView::setAllSpans(const QModelIndex &index)
{
	const auto *m = index.model();
	setFirstColumnSpanned(index.row(), m->parent(index), m->span(index).width() != 1);
	if (m->hasChildren(index)) {
		setAllSpans(m->index(0, 0, index));
	}
}

void SpanAwareTreeView::reset()
{
	QTreeView::reset();

	if (auto *m = model()) {
		for (auto row = 0; row < m->rowCount(); ++row) {
			setAllSpans(m->index(row, 0));
		}
	}
}

} // namespace widgets
