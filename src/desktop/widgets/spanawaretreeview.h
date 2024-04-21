// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_WIDGETS_SPANAWARETREEVIEW_H
#define DESKTOP_WIDGETS_SPANAWARETREEVIEW_H

#include <QList>
#include <QTreeView>
#include <QVector>

#ifdef DESIGNER_PLUGIN
#	include <QtUiPlugin/QDesignerExportWidget>
#else
#	define QDESIGNER_WIDGET_EXPORT
#endif

class QModelIndex;

namespace widgets {

class QDESIGNER_WIDGET_EXPORT SpanAwareTreeView final : public QTreeView {
	Q_OBJECT
public:
	using QTreeView::QTreeView;
	void reset() override;

signals:
	void verticalScrollBarVisibilityChanged();

protected:
	void dataChanged(
		const QModelIndex &topLeft, const QModelIndex &bottomRight,
		const QVector<int> &roles = QVector<int>()) override;
	void rowsInserted(const QModelIndex &parent, int start, int end) override;
	void updateGeometries() override;

private:
	void setAllSpans(const QModelIndex &index);

	bool m_verticalScrollBarVisible = false;
};

} // namespace widgets
#endif
