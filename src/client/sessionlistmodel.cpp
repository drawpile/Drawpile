/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "sessionlistmodel.h"

SessionListModel::SessionListModel(const network::SessionList& list, QObject *parent)
	: QAbstractTableModel(parent), sessions_(list)
{
}

QVariant SessionListModel::headerData(int section, Qt::Orientation orientation,
		int role) const
{
	if(role == Qt::DisplayRole && orientation==Qt::Horizontal) {
		if(section==0)
			return tr("Title");
		else if(section==1)
			return tr("Width");
		else if(section==2)
			return tr("Height");
	}
	return QVariant();
}

QVariant SessionListModel::data(const QModelIndex& index, int role) const
{
	if(index.row() < 0 || index.row() >= sessions_.count())
		return QVariant();
	if(role == Qt::DisplayRole) {
		if(index.column() == 0)
			return sessions_.at(index.row()).title;
		else if(index.column() == 1)
			return sessions_.at(index.row()).width;
		else if(index.column() == 2)
			return sessions_.at(index.row()).height;
	}
	return QVariant();
}

int SessionListModel::rowCount(const QModelIndex& parent) const
{
	if(parent.isValid())
		return 0;
	return sessions_.count();
}

int SessionListModel::columnCount(const QModelIndex&) const
{
	return 3;
}

