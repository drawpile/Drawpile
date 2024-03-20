// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/serverlogview.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>

namespace widgets {

ServerLogView::ServerLogView(QWidget *parent)
	: QListView(parent)
{
}

void ServerLogView::keyPressEvent(QKeyEvent *event)
{
	QListView::keyPressEvent(event);
	if(event == QKeySequence::Copy && model() && selectionModel()) {
		const QModelIndexList indexes = selectedIndexes();
		QStringList values;
		if(!indexes.isEmpty()) {
			for(const QModelIndex &index : indexes) {
				if(index.isValid()) {
					values.append(
						model()->data(index, Qt::DisplayRole).toString());
				}
			}
			QGuiApplication::clipboard()->setText(values.join('\n'));
		}
		event->accept();
	}
}

}
