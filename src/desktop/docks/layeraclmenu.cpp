/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2018 Calle Laakkonen

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

#include "docks/layeraclmenu.h"
#include "canvas/userlist.h"
#include "parentalcontrols/parentalcontrols.h"

#include <QApplication>

namespace docks {

static void addTier(QActionGroup *group, const QString &title, canvas::Tier tier)
{
	QAction *a = group->addAction(title);
	a->setProperty("userTier", int(tier));
	a->setCheckable(true);
	a->setChecked(true);
}

LayerAclMenu::LayerAclMenu(QWidget *parent) :
    QMenu(parent)
{
	m_lock = addAction(tr("Lock this layer"));
	m_lock->setCheckable(true);

	m_censored = addAction(tr("Censor"));
	m_censored->setCheckable(true);

	addSection(tr("Access tier:"));
	m_tiers = new QActionGroup(this);
	addTier(m_tiers, tr("Operators"), canvas::Tier::Op);
	addTier(m_tiers, tr("Trusted"), canvas::Tier::Trusted);
	addTier(m_tiers, tr("Registered"), canvas::Tier::Auth);
	addTier(m_tiers, tr("Everyone"), canvas::Tier::Guest);
	static_assert(canvas::TierCount == 4, "update LayerAclMenu tiers!");
	addActions(m_tiers->actions());

	addSection(tr("Exclusive access:"));
	m_users = new QActionGroup(this);
	m_users->setExclusive(false);

	connect(this, &LayerAclMenu::triggered, this, &LayerAclMenu::userClicked);

	connect(qApp, SIGNAL(settingsChanged()), this, SLOT(refreshParentalControls()));
	refreshParentalControls();
}

void LayerAclMenu::refreshParentalControls()
{
	m_censored->setEnabled(!parentalcontrols::isLayerUncensoringBlocked());
}

void LayerAclMenu::setUserList(QAbstractItemModel *model)
{
	m_model = model;
	for(int i=0;i<model->rowCount();++i)
		addUser(i);

	connect(model, &canvas::UserListModel::rowsInserted, this, &LayerAclMenu::rowsInserted);
	connect(model, &canvas::UserListModel::rowsMoved, this, &LayerAclMenu::rowsMoved);
	connect(model, &canvas::UserListModel::rowsRemoved, this, &LayerAclMenu::rowsRemoved);
	connect(model, &canvas::UserListModel::modelReset, this, &LayerAclMenu::rowsReset);
}

void LayerAclMenu::addUser(int index)
{
	const QModelIndex uidx = m_model->index(index, 0);

	QAction *userAction = m_users->addAction(uidx.data(canvas::UserListModel::NameRole).toString());
	userAction->setCheckable(true);
	userAction->setProperty("userId", uidx.data(canvas::UserListModel::IdRole).toInt());

	addAction(userAction);
}

void LayerAclMenu::rowsInserted(const QModelIndex &parent, int start, int end)
{
	Q_UNUSED(parent);
	Q_ASSERT(start == m_users->actions().size()); // new users are always added to the end of the list
	for(;start<=end;++start)
		addUser(start);
}

void LayerAclMenu::rowsMoved(const QModelIndex&, int, int, const QModelIndex&, int)
{
	// Users are never moved in the list
	qWarning("rowsMoved not implemented!");
}

void LayerAclMenu::rowsRemoved(const QModelIndex &parent, int start, int end)
{
	Q_UNUSED(parent);
	Q_ASSERT(end < m_users->actions().size());
	for(int i=start;i<=end;++i) {
		delete m_users->actions()[start];
	}
}

void LayerAclMenu::rowsReset()
{
	QList<QAction*> actions =m_users->actions();
	for(auto *a : actions)
		delete a;
}

void LayerAclMenu::userClicked(QAction *useraction)
{
	// Get exclusive user access list
	QList<uint8_t> exclusive;
	for(const QAction *a : m_users->actions()) {
		if(a->isChecked())
			exclusive.append(a->property("userId").toInt());
	}

	// Get selected tier
	canvas::Tier tier = canvas::Tier::Guest;
	for(const QAction *a : m_tiers->actions()) {
		if(a->isChecked()) {
			tier = canvas::Tier(a->property("userTier").toInt());
			break;
		}
	}

	if(useraction == m_lock) {
		// Lock out all other controls when general layer lock is on
		const bool enable = !useraction->isChecked();
		m_tiers->setEnabled(enable && exclusive.isEmpty());
		m_users->setEnabled(enable);

	} else if(useraction == m_censored) {
		// Just toggle the censored flag, no other ACL changes
		emit layerCensoredChange(m_censored->isChecked());
		return;

	} else {
		// User exclusive access bit or tier changed.
		m_tiers->setEnabled(exclusive.isEmpty());
	}

	// Send ACL update message
	emit layerAclChange(m_lock->isChecked(), tier, exclusive);
}

void LayerAclMenu::setAcl(bool lock, canvas::Tier tier, const QList<uint8_t> exclusive)
{
	m_lock->setChecked(lock);

	m_users->setEnabled(!lock);
	m_tiers->setEnabled(!lock && exclusive.isEmpty());

	for(QAction *t : m_tiers->actions()) {
		if(t->property("userTier").toInt() == int(tier)) {
			t->setChecked(true);
			break;
		}
	}

	for(QAction *u : m_users->actions()) {
		u->setChecked(exclusive.contains(u->property("userId").toInt()));
	}
}

void LayerAclMenu::setCensored(bool censor)
{
	m_censored->setChecked(censor);
}

}

