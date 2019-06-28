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

#include "lasertrailmodel.h"

#include <QTimerEvent>
#include <QDateTime>

namespace canvas {

LaserTrailModel::LaserTrailModel(QObject *parent)
	: QAbstractListModel(parent), m_lastId(0)
{
	m_timerId = startTimer(1000, Qt::VeryCoarseTimer);
}

int LaserTrailModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_lasers.size();
}

QVariant LaserTrailModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && index.row() >= 0 && index.row() < m_lasers.size()) {
		const LaserTrail &lt = m_lasers.at(index.row());
		switch(role) {
		case ColorRole: return lt.color;
		case ContextRole: return lt.ctxid;
		case PointsRole: return QVariant::fromValue(lt.points);
		case VisibleRole: return lt.visible;
		case InternalIdRole: return lt.internalId;
		default: break;
		}
	}
	return QVariant();
}

QHash<int, QByteArray> LaserTrailModel::roleNames() const
{
	QHash<int, QByteArray> roles;
	roles[ColorRole] = "color";
	roles[ContextRole] = "context";
	roles[PointsRole] = "points";
	roles[VisibleRole] = "visible";
	return roles;
}

void LaserTrailModel::timerEvent(QTimerEvent *e)
{
	if(e->timerId() != m_timerId) {
		QAbstractListModel::timerEvent(e);
		return;
	}

	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	const qint64 removeTreshold = now - 5*1000;

	int idx=0;

	QMutableListIterator<LaserTrail> i(m_lasers);
	while(i.hasNext()) {
		LaserTrail &lt = i.next();

		if(lt.expiration < removeTreshold) {
			beginRemoveRows(QModelIndex(), idx, idx);
			i.remove();
			endRemoveRows();

		} else {
			if(lt.visible && lt.expiration < now) {
				lt.visible = false;
				lt.open = false;
				emit dataChanged(index(idx), index(idx), QVector<int>() << VisibleRole);
			}

			++idx;
		}
	}
}

void LaserTrailModel::startTrail(int ctxId, const QColor &color, int persistence)
{
	if(persistence==0) {
		endTrail(ctxId);
		return;
	}

	// Only one open trail per user is allowed, so starting a new trail
	// automatically closes the previous trail if it exists.
	for(int i=0;i<m_lasers.size();++i) {
		if(m_lasers.at(i).ctxid == ctxId && m_lasers.at(i).open) {
			m_lasers[i].open = false;
			break;
		}
	}

	beginInsertRows(QModelIndex(), m_lasers.size(), m_lasers.size());
	m_lasers.append(LaserTrail {
		ctxId,
		++m_lastId,
		color,
		QVector<QPointF>(),
		true,
		true,
		persistence,
		QDateTime::currentMSecsSinceEpoch() + persistence * 1000
	});
	endInsertRows();
}

void LaserTrailModel::addPoint(int ctxId, const QPointF &point)
{
	for(int i=0;i<m_lasers.size();++i) {
		if(m_lasers.at(i).ctxid == ctxId && m_lasers.at(i).open) {
			m_lasers[i].points << point;
			m_lasers[i].expiration = QDateTime::currentMSecsSinceEpoch() + m_lasers[i].persistence * 1000;
			QModelIndex idx = index(i);
			emit dataChanged(idx, idx, QVector<int>() << PointsRole);
			break;
		}
	}
}

void LaserTrailModel::endTrail(int ctxId)
{
	for(int i=0;i<m_lasers.size();++i) {
		if(m_lasers.at(i).ctxid == ctxId && m_lasers.at(i).open) {
			m_lasers[i].open = false;

			// any given context ID can have only one open trail at a time
			break;
		}
	}
}

bool LaserTrailModel::isOpenTrail(int ctxId)
{
	for(int i=0;i<m_lasers.size();++i)
		if(m_lasers.at(i).ctxid == ctxId && m_lasers.at(i).open)
			return true;
	return false;
}

}
