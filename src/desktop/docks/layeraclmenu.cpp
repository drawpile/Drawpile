// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/layeraclmenu.h"
#include "desktop/main.h"
#include "libclient/canvas/userlist.h"
#include "libclient/parentalcontrols/parentalcontrols.h"
#include <QActionGroup>

namespace docks {

LayerAclMenu::LayerAclMenu(QWidget *parent)
	: QMenu(parent)
	, m_userlist(nullptr)
{
	m_lock = addAction(tr("Lock this layer"));
	m_lock->setCheckable(true);
	connect(m_lock, &QAction::triggered, this, &LayerAclMenu::layerLockChange);

	m_censored = addAction(tr("Censor"));
	m_censored->setCheckable(true);
	connect(
		m_censored, &QAction::triggered, this,
		&LayerAclMenu::layerCensoredChange);

	addSection(tr("Access tier:"));
	m_tiers = new QActionGroup(this);
	static_assert(DP_ACCESS_TIER_COUNT == 4, "update LayerAclMenu tiers!");
	QPair<QString, int> pairs[DP_ACCESS_TIER_COUNT] = {
		{tr("Operators"), int(DP_ACCESS_TIER_OPERATOR)},
		{tr("Trusted"), int(DP_ACCESS_TIER_TRUSTED)},
		{tr("Registered"), int(DP_ACCESS_TIER_AUTHENTICATED)},
		{tr("Everyone"), int(DP_ACCESS_TIER_GUEST)},
	};
	for(QPair<QString, int> p : pairs) {
		QAction *action = m_tiers->addAction(p.first);
		action->setProperty("userTier", p.second);
		action->setCheckable(true);
		connect(action, &QAction::triggered, this, [this, tier = p.second] {
			emit layerAccessTierChange(tier);
		});
	}
	addActions(m_tiers->actions());

	addSection(tr("Exclusive access:"));
	m_users = new QActionGroup(this);
	m_users->setExclusive(false);

	dpApp().settings().bindParentalControlsForceCensor(this, [=](bool force) {
		m_censored->setDisabled(force);
	});
}

void LayerAclMenu::setUserList(QAbstractItemModel *model)
{
	m_userlist = model;
}

void LayerAclMenu::showEvent(QShowEvent *e)
{
	// Rebuild user list when menu is shown
	const QList<QAction *> actions = m_users->actions();
	for(QAction *a : actions) {
		delete a;
	}

	if(m_userlist) {
		for(int i = 0; i < m_userlist->rowCount(); ++i) {
			QModelIndex idx = m_userlist->index(i, 0);
			int userId = idx.data(canvas::UserListModel::IdRole).toInt();
			QAction *ua = m_users->addAction(
				idx.data(canvas::UserListModel::NameRole).toString());
			ua->setCheckable(true);
			ua->setChecked(m_exclusives.contains(userId));
			addAction(ua);
			connect(
				ua, &QAction::triggered, this, [this, userId](bool checked) {
					emit layerUserAccessChanged(userId, checked);
				});
		}
	}

	QMenu::showEvent(e);
}

void LayerAclMenu::setAcl(bool lock, int tier, const QVector<uint8_t> exclusive)
{
	m_lock->setChecked(lock);

	m_users->setEnabled(!lock);
	m_tiers->setEnabled(!lock);

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
