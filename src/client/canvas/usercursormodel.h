/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#ifndef USERCURSORMODEL_H
#define USERCURSORMODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QPointF>
#include <QList>

namespace canvas {

class LayerListModel;

struct UserCursor {
	int id;
	bool visible;
	qint64 lastMoved;
	int layerId;

	QPoint pos;
	QString name;
	QString layer;
	QColor color;
};

class UserCursorModel : public QAbstractListModel
{
	Q_OBJECT
public:
	enum UserCursorRoles {
		// DisplayRole is used to get the name
		IdRole = Qt::UserRole + 10,
		PositionRole,
		LayerRole,
		ColorRole,
		VisibleRole
	};

	explicit UserCursorModel(QObject *parent=nullptr);

	/**
	 * @brief Set the layer list model.
	 *
	 * Layer names are read from here
	 */
	void setLayerList(LayerListModel *layers) { m_layerlist = layers; }

	int rowCount(const QModelIndex &parent=QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const;

	QHash<int, QByteArray> roleNames() const;

	QModelIndex indexForId(int id) const;

public slots:
	void setCursorName(int id, const QString &name);
	void setCursorColor(int id, const QColor &color);
	void setCursorPosition(int id, int layerId, const QPoint &pos);
	void hideCursor(int id);

	void clear();

protected:
	void timerEvent(QTimerEvent *e);

private:
	UserCursor *getOrCreate(int id, QModelIndex &index);

	QList<UserCursor> m_cursors;
	LayerListModel *m_layerlist;
	int m_timerId;
};

}

#endif // USERCURSORMODEL_H
