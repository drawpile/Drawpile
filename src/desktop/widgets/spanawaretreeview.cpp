// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/spanawaretreeview.h"
#include <QDebug>
#include <QList>
#include <QModelIndex>
#include <QScrollBar>

namespace widgets {

void SpanAwareTreeView::dataChanged(
	const QModelIndex &topLeft, const QModelIndex &bottomRight,
	const QVector<int> &roles)
{
	QTreeView::dataChanged(topLeft, bottomRight, roles);
	if(const QAbstractItemModel *m = model()) {
		for(int row = topLeft.row(); row <= bottomRight.row(); ++row) {
			setAllSpans(m->index(row, 0, topLeft.parent()));
		}
	}
}

void SpanAwareTreeView::rowsInserted(
	const QModelIndex &parent, int start, int end)
{
	QTreeView::rowsInserted(parent, start, end);
	if(const QAbstractItemModel *m = model()) {
		for(int row = start; row <= end; ++row) {
			setAllSpans(m->index(row, 0, parent));
		}
	}
}

void SpanAwareTreeView::updateGeometries()
{
	QTreeView::updateGeometries();
	QScrollBar *vbar = verticalScrollBar();
	bool verticalScrollBarVisible = vbar ? vbar->isVisible() : false;
	if(m_verticalScrollBarVisible != verticalScrollBarVisible) {
		m_verticalScrollBarVisible = verticalScrollBarVisible;
		emit verticalScrollBarVisibilityChanged();
	}
}

void SpanAwareTreeView::setAllSpans(const QModelIndex &index)
{
	const QAbstractItemModel *m = index.model();
	setFirstColumnSpanned(
		index.row(), m->parent(index), m->span(index).width() != 1);
	if(m->hasChildren(index)) {
		setAllSpans(m->index(0, 0, index));
	}
}

void SpanAwareTreeView::reset()
{
	QTreeView::reset();
	if(QAbstractItemModel *m = model()) {
		int rowCount = m->rowCount();
		for(int row = 0; row < rowCount; ++row) {
			setAllSpans(m->index(row, 0));
		}
	}
}

}
