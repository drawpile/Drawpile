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

#include "desktop/docks/layeraclmenu.h"
#include "libclient/canvas/userlist.h"
#include "libclient/parentalcontrols/parentalcontrols.h"

#include <QApplication>
#include <QActionGroup>

namespace docks {

static void addTier(QActionGroup *group, const QString &title, canvas::Tier tier)
{
	QAction *a = group->addAction(title);
	a->setProperty("userTier", int(tier));
	a->setCheckable(true);
	a->setChecked(true);
}

LayerAclMenu::LayerAclMenu(QWidget *parent) :
	QMenu(parent), m_userlist(nullptr)
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
	m_userlist = model;
}

void LayerAclMenu::showEvent(QShowEvent *e)
{
	// Rebuild user list when menu is shown
	const QList<QAction*> actions = m_users->actions();
	for(auto *a : actions)
		delete a;

	if(m_userlist) {
		for(int i=0;i<m_userlist->rowCount();++i) {
			const QModelIndex idx = m_userlist->index(i, 0);
			const int id = idx.data(canvas::UserListModel::IdRole).toInt();

			QAction *ua = m_users->addAction(idx.data(canvas::UserListModel::NameRole).toString());
			ua->setCheckable(true);
			ua->setProperty("userId", id);
			ua->setChecked(m_exclusives.contains(id));
			addAction(ua);
		}
	}

	QMenu::showEvent(e);
}

void LayerAclMenu::userClicked(QAction *useraction)
{
	// Get exclusive user access list
	QVector<uint8_t> exclusive;
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

void LayerAclMenu::setAcl(bool lock, int tier, const QVector<uint8_t> exclusive)
{
	m_lock->setChecked(lock);

	m_users->setEnabled(!lock);
	m_tiers->setEnabled(!lock && exclusive.isEmpty());

	for(QAction *t : m_tiers->actions()) {
		if(t->property("userTier").toInt() == tier) {
			t->setChecked(true);
			break;
		}
	}

	m_exclusives = exclusive;
}

void LayerAclMenu::setCensored(bool censor)
{
	m_censored->setChecked(censor);
}

}

