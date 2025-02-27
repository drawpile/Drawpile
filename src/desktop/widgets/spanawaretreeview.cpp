// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/spanawaretreeview.h"
#include <QDebug>
#include <QKeyEvent>
#include <QKeySequence>
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
	checkVerticalScrollBarVisibility();
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
	checkVerticalScrollBarVisibility();
}

void SpanAwareTreeView::updateGeometries()
{
	QTreeView::updateGeometries();
	checkVerticalScrollBarVisibility();
}

void SpanAwareTreeView::verticalScrollbarValueChanged(int value)
{
	QTreeView::verticalScrollbarValueChanged(value);
	checkVerticalScrollBarVisibility();
}

void SpanAwareTreeView::keyPressEvent(QKeyEvent *event)
{
	if(m_handleCopy && event == QKeySequence::Copy) {
		emit copyRequested();
		event->accept();
	} else if(m_handleDelete && event == QKeySequence::Delete) {
		emit deleteRequested();
		event->accept();
	} else {
		QTreeView::keyPressEvent(event);
	}
}

void SpanAwareTreeView::setAllSpans(const QModelIndex &index)
{
	const QAbstractItemModel *m = index.model();
	if(m) {
		setFirstColumnSpanned(
			index.row(), m->parent(index), m->span(index).width() != 1);
		if(m->hasChildren(index)) {
			int childRows = m->rowCount(index);
			for(int i = 0; i < childRows; ++i) {
				setAllSpans(m->index(i, 0, index));
			}
		}
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
	checkVerticalScrollBarVisibility();
}

void SpanAwareTreeView::checkVerticalScrollBarVisibility()
{
	QScrollBar *vbar = verticalScrollBar();
	bool verticalScrollBarVisible = vbar ? vbar->isVisible() : false;
	if(m_verticalScrollBarVisible != verticalScrollBarVisible) {
		m_verticalScrollBarVisible = verticalScrollBarVisible;
		emit verticalScrollBarVisibilityChanged();
	}
}

}
