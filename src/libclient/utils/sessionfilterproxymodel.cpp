/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#include "libclient/utils/sessionfilterproxymodel.h"
#include "libclient/net/sessionlistingmodel.h"
#include "libclient/net/loginsessions.h"

SessionFilterProxyModel::SessionFilterProxyModel(QObject *parent)
	: QSortFilterProxyModel(parent),
	m_showPassworded(true),
	m_showNsfw(true),
	m_showClosed(true)
{
}

void SessionFilterProxyModel::setShowPassworded(bool show)
{
	if(m_showPassworded != show) {
		m_showPassworded = show;
		invalidateFilter();
	}
}

void SessionFilterProxyModel::setShowNsfw(bool show)
{
	if(m_showNsfw != show) {
		m_showNsfw = show;
		invalidateFilter();
	}
}

void SessionFilterProxyModel::setShowClosed(bool show)
{
	if(m_showClosed != show) {
		m_showClosed = show;
		invalidateFilter();
	}
}

bool SessionFilterProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
	const QModelIndex i = sourceModel()->index(source_row, 0, source_parent);
	int nsfwRole=0, pwRole=0, closedRole=0;

	if(sourceModel()->inherits(SessionListingModel::staticMetaObject.className())) {
		// Always show the top level items (listin servers)
		if(!source_parent.isValid())
			return true;

		nsfwRole = SessionListingModel::IsNsfwRole;
		pwRole = SessionListingModel::IsPasswordedRole;
		closedRole = SessionListingModel::IsClosedRole;

	} else if(sourceModel()->inherits(net::LoginSessionModel::staticMetaObject.className())) {
		nsfwRole = net::LoginSessionModel::NsfmRole;
		pwRole = net::LoginSessionModel::NeedPasswordRole;
		closedRole = net::LoginSessionModel::ClosedRole;
	}

	if(!m_showNsfw && nsfwRole) {
		if(i.data(nsfwRole).toBool())
			return false;
	}

	if(!m_showPassworded && pwRole) {
		if(i.data(pwRole).toBool())
			return false;
	}

	if(!m_showClosed && closedRole) {
		if(i.data(pwRole).toBool())
			return false;
	}

	return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent);
}

