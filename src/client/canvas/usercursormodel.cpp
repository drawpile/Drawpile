/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2018 Calle Laakkonen

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

#include "usercursormodel.h"
#include "layerlist.h"

#include <QDateTime>
#include <QTimerEvent>
#include <QDebug>

namespace canvas {

UserCursorModel::UserCursorModel(QObject *parent)
	: QAbstractListModel(parent), m_layerlist(nullptr)
{
	m_timerId = startTimer(1000, Qt::VeryCoarseTimer);
}

int UserCursorModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_cursors.size();
}

QModelIndex UserCursorModel::indexForId(int id) const
{
	for(int i=0;i<m_cursors.size();++i)
		if(m_cursors.at(i).id == id)
			return index(i);
	return QModelIndex();
}

QVariant UserCursorModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && index.row() >= 0 && index.row() < m_cursors.size()) {
		const UserCursor &uc = m_cursors.at(index.row());
	switch(role) {
		case Qt::DisplayRole: return uc.name;
		case Qt::DecorationRole: return uc.avatar;
		case IdRole: return uc.id;
		case PositionRole: return uc.pos;
		case LayerRole: return uc.layer;
		case ColorRole: return uc.color;
		case VisibleRole: return uc.visible;
		default: break;
		}
	}
	return QVariant();
}

QHash<int, QByteArray> UserCursorModel::roleNames() const
{
	QHash<int, QByteArray> roles;
	roles[Qt::DisplayRole] = "display";
	roles[Qt::DecorationRole] = "decoration";
	roles[IdRole] = "id";
	roles[PositionRole] = "pos";
	roles[LayerRole] = "layer";
	roles[ColorRole] = "color";
	roles[VisibleRole] = "visible";
	return roles;
}


void UserCursorModel::setCursorName(int id, const QString &name)
{
	QModelIndex index;
	UserCursor *uc = getOrCreate(id, index);

	uc->name = name;

	emit dataChanged(index, index, QVector<int>() << Qt::DisplayRole);
}

void UserCursorModel::setCursorAvatar(int id, const QPixmap &avatar)
{
	QModelIndex index;
	UserCursor *uc = getOrCreate(id, index);

	uc->avatar = avatar;

	emit dataChanged(index, index, QVector<int>() << Qt::DecorationRole);
}

void UserCursorModel::setCursorColor(int id, const QColor &color)
{
	QModelIndex index;
	UserCursor *uc = getOrCreate(id, index);

	uc->color = color;

	emit dataChanged(index, index, QVector<int>() << ColorRole);
}

void UserCursorModel::setCursorPosition(int id, int layerId, const QPoint &pos)
{
	QModelIndex index;
	UserCursor *uc = getOrCreate(id, index);

	QVector<int> roles;

	uc->pos = pos; roles << PositionRole;
	uc->lastMoved = QDateTime::currentMSecsSinceEpoch();
	if(!uc->visible) {
		uc->visible = true;
		roles << VisibleRole;
	}

	if(layerId>0 && layerId != uc->layerId) {
		uc->layerId = layerId;
		QString layerName = QStringLiteral("???");
		if(m_layerlist) {
			QVariant ln = m_layerlist->layerIndex(layerId).data(LayerListModel::TitleRole);
			if(!ln.isNull())
				layerName = ln.toString();
		}
		uc->layer = layerName;
		roles << LayerRole;
	}

	emit dataChanged(index, index, roles);
}

void UserCursorModel::hideCursor(int id)
{
	for(int i=0;i<m_cursors.size();++i) {
		if(m_cursors.at(i).id == id) {
			m_cursors[i].visible = false;
			QModelIndex idx = index(i);
			emit dataChanged(idx, idx, QVector<int>() << VisibleRole);
			return;
		}
	}
}

void UserCursorModel::clear()
{
	beginResetModel();
	m_cursors.clear();
	endResetModel();
}

UserCursor *UserCursorModel::getOrCreate(int id, QModelIndex &idx)
{
	for(int i=0;i<m_cursors.size();++i) {
		if(m_cursors.at(i).id == id) {
			idx = index(i);
			return &m_cursors[i];
		}
	}

	beginInsertRows(QModelIndex(), m_cursors.size(), m_cursors.size());
	m_cursors.append(UserCursor {
		id,
		false,
		QDateTime::currentMSecsSinceEpoch(),
		0,
		QPoint(),
		QStringLiteral("#%1").arg(id),
		QString(),
		QColor(Qt::black),
		QPixmap()
	});
	endInsertRows();

	idx = index(m_cursors.size()-1);
	return &m_cursors[m_cursors.size()-1];
}

void UserCursorModel::timerEvent(QTimerEvent *e)
{
	if(e->timerId() != m_timerId) {
		QAbstractListModel::timerEvent(e);
		return;
	}

	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	const qint64 hideTreshold = now - 3000;

	for(int i=0;i<m_cursors.size();++i) {
		UserCursor &uc = m_cursors[i];

		if(uc.visible && uc.lastMoved < hideTreshold) {
			uc.visible = false;
			QModelIndex idx = index(i);
			emit dataChanged(idx, idx, QVector<int>() << VisibleRole);
		}
	}
}

}
