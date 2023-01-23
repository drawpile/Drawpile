/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2019 Calle Laakkonen

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
#ifndef LAYERACLMENU_H
#define LAYERACLMENU_H

#include "libclient/canvas/features.h"

#include <QMenu>

class QAbstractItemModel;

namespace docks {

class LayerAclMenu : public QMenu
{
    Q_OBJECT
public:
	explicit LayerAclMenu(QWidget *parent=nullptr);

	void setUserList(QAbstractItemModel *model);
	void setAcl(bool lock, int tier, const QVector<uint8_t> acl);
	void setCensored(bool censor);

signals:
	/**
	 * @brief Layer Access Control List changed
	 *
	 * This signal includes the new exclusive access list.
	 * The list is empty if all users have access.
	 *
	 * @param lock general layer lock
	 * @param ids list of user IDs.
	 */
	void layerAclChange(bool lock, canvas::Tier tier, QVector<uint8_t> ids);

	/**
	 * @brief The censored checkbox was toggled
	 */
	void layerCensoredChange(bool censor);

protected:
	void showEvent(QShowEvent *e);

private slots:
	void userClicked(QAction *useraction);
	void refreshParentalControls();

private:
	QAbstractItemModel *m_userlist;
	QVector<uint8_t> m_exclusives;
	QAction *m_lock;
	QAction *m_censored;
	QActionGroup *m_tiers;
	QActionGroup *m_users;
};

}

#endif // LAYERACLMENU_H
