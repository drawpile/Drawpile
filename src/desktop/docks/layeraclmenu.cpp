// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/docks/layeraclmenu.h"
#include "libclient/canvas/userlist.h"
#include "libclient/parentalcontrols/parentalcontrols.h"

#include <QApplication>
#include <QActionGroup>

namespace docks {

static void addTier(QActionGroup *group, const QString &title, DP_AccessTier tier)
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
	addTier(m_tiers, tr("Operators"), DP_ACCESS_TIER_OPERATOR);
	addTier(m_tiers, tr("Trusted"), DP_ACCESS_TIER_TRUSTED);
	addTier(m_tiers, tr("Registered"), DP_ACCESS_TIER_AUTHENTICATED);
	addTier(m_tiers, tr("Everyone"), DP_ACCESS_TIER_GUEST);
	static_assert(DP_ACCESS_TIER_COUNT == 4, "update LayerAclMenu tiers!");
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
	DP_AccessTier tier = DP_ACCESS_TIER_GUEST;
	for(const QAction *a : m_tiers->actions()) {
		if(a->isChecked()) {
			tier = DP_AccessTier(a->property("userTier").toInt());
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

