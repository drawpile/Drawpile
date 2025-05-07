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
	m_alphaLock = addAction(tr("Alpha lock layer for you"));
	m_alphaLock->setCheckable(true);
	connect(
		m_alphaLock, &QAction::triggered, this,
		&LayerAclMenu::layerAlphaLockChange);

	m_censored = addAction(tr("Censor layer"));
	m_censored->setCheckable(true);
	connect(
		m_censored, &QAction::triggered, this,
		&LayerAclMenu::layerCensoredChange);

	addSection(tr("Locks:"));

	m_lockAll = addAction(tr("Lock layer entirely"));
	m_lockAll->setCheckable(true);
	connect(
		m_lockAll, &QAction::triggered, this,
		&LayerAclMenu::layerLockAllChange);

	addSeparator();

	m_contentLock = addAction(tr("Lock layer content"));
	m_contentLock->setCheckable(true);
	connect(
		m_contentLock, &QAction::triggered, this,
		&LayerAclMenu::layerContentLockChange);

	m_propsLock = addAction(tr("Lock layer properties"));
	m_propsLock->setCheckable(true);
	connect(
		m_propsLock, &QAction::triggered, this,
		&LayerAclMenu::layerPropsLockChange);

	m_moveLock = addAction(tr("Lock layer position"));
	m_moveLock->setCheckable(true);
	connect(
		m_moveLock, &QAction::triggered, this,
		&LayerAclMenu::layerMoveLockChange);

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
			ua->setChecked(m_exclusive.contains(userId));
			addAction(ua);
			connect(
				ua, &QAction::triggered, this, [this, userId](bool checked) {
					emit layerUserAccessChanged(userId, checked);
				});
		}
	}

	QMenu::showEvent(e);
}

void LayerAclMenu::setAcl(
	bool contentLock, bool propsLock, bool moveLock, int tier,
	const QVector<uint8_t> exclusive)
{
	m_contentLock->setChecked(contentLock);
	m_propsLock->setChecked(propsLock);
	m_moveLock->setChecked(moveLock);
	m_lockAll->setChecked(contentLock && propsLock && moveLock);

	m_allUsersLocked = contentLock;
	m_users->setEnabled(!m_allUsersLocked && m_canEdit);
	m_tiers->setEnabled(!m_allUsersLocked && m_canEdit);

	for(QAction *t : m_tiers->actions()) {
		if(t->property("userTier").toInt() == tier) {
			t->setChecked(true);
			break;
		}
	}

	m_exclusive = exclusive;
}

void LayerAclMenu::setCensored(bool censor)
{
	m_censored->setChecked(censor);
}

void LayerAclMenu::setAlphaLock(bool alphaLock)
{
	m_alphaLock->setChecked(alphaLock);
}

void LayerAclMenu::setAlphaLockEnabled(bool alphaLockEnabled)
{
	m_alphaLock->setEnabled(alphaLockEnabled);
}

void LayerAclMenu::setCanEdit(bool canEdit, bool compatibilityMode)
{
	if(m_canEdit != canEdit || m_compatibilityMode != compatibilityMode) {
		m_canEdit = canEdit;
		m_compatibilityMode = compatibilityMode;
		m_censored->setEnabled(m_canEdit);
		m_contentLock->setEnabled(m_canEdit);
		m_propsLock->setEnabled(m_canEdit && !m_compatibilityMode);
		m_moveLock->setEnabled(m_canEdit && !m_compatibilityMode);
		m_lockAll->setEnabled(m_canEdit && !m_compatibilityMode);
		m_users->setEnabled(!m_allUsersLocked && m_canEdit);
		m_tiers->setEnabled(!m_allUsersLocked && m_canEdit);
	}
}

}
