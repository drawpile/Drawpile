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

#ifndef LASERTRAILMODEL_H
#define LASERTRAILMODEL_H

#include <QAbstractListModel>
#include <QColor>
#include <QPointF>

namespace canvas {

struct LaserTrail {
	int ctxid;
	int internalId;
	QColor color;
	QVector<QPointF> points;
	bool open;
	bool visible;

	int persistence;
	qint64 expiration;
};

class LaserTrailModel : public QAbstractListModel
{
	Q_OBJECT
public:
	enum LaserRolesRoles {
		ColorRole = Qt::UserRole + 1,
		ContextRole,
		PointsRole,
		VisibleRole,
		InternalIdRole
	};

	explicit LaserTrailModel(QObject *parent=nullptr);

	int rowCount(const QModelIndex &parent=QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const;

	QHash<int, QByteArray> roleNames() const;

public slots:
	void startTrail(int ctxId, const QColor &color, int persistence);
	void addPoint(int ctxId, const QPointF &point);
	void endTrail(int ctxId);

protected:
	void timerEvent(QTimerEvent *e);

private:
	bool isOpenTrail(int ctxId);
	QList<LaserTrail> m_lasers;
	int m_timerId;
	int m_lastId;
};

}

#endif // LASERTRAILMODEL_H
