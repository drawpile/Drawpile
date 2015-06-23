/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef LAYERLISTMODEL_H
#define LAYERLISTMODEL_H

#include <QAbstractListModel>
#include <QItemDelegate>

namespace net {
	class Client;
	struct LayerListItem;
}
namespace docks {

/**
 * \brief A custom item delegate for displaying layer names and editing layer settings.
 */
class LayerListDelegate : public QItemDelegate {
Q_OBJECT
public:
	enum TitleMode { SHOW_TITLE, SHOW_NUMBER };

	LayerListDelegate(QObject *parent=0);

	void paint(QPainter *painter, const QStyleOptionViewItem &option,
			const QModelIndex &index) const;
	QSize sizeHint(const QStyleOptionViewItem & option,
			const QModelIndex & index ) const;

	void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
	void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex& index) const;
	bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index);

	void setClient(net::Client *client) { _client = client; }

	TitleMode titleMode() const { return _titlemode; }
	void setTitleMode(TitleMode tm) { _titlemode = tm; }

signals:
	void toggleVisibility(int layerId, bool visible);

private:
	void drawOpacityGlyph(const QRectF& rect, QPainter *painter, float value, bool hidden) const;

	net::Client *_client;
	QPixmap _visibleicon;
	QPixmap _hiddenicon;

	TitleMode _titlemode;
};

}

#endif
