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

#ifndef SESSIONFILTERPROXYMODEL_H
#define SESSIONFILTERPROXYMODEL_H

#include <QSortFilterProxyModel>

/**
 * A custom SortFilter proxy model that has special support for
 *  - SessionListingModel
 *  - LoginSessionModel
 *
 * Otherwise works like normal QSortFilterProxyModel
 */
class SessionFilterProxyModel final : public QSortFilterProxyModel
{
	Q_OBJECT
public:
	SessionFilterProxyModel(QObject *parent=nullptr);

	bool showNsfw() const { return m_showNsfw; }
	bool showPassworded() const { return m_showPassworded; }

public slots:
	void setShowNsfw(bool show);
	void setShowPassworded(bool show);
	void setShowClosed(bool show);

protected:
	bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
	bool m_showPassworded;
	bool m_showNsfw;
	bool m_showClosed;
};

#endif

