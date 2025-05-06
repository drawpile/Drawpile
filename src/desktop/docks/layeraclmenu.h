// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LAYERACLMENU_H
#define LAYERACLMENU_H

extern "C" {
#include <dpmsg/acl.h>
}

#include <QMenu>

class QAbstractItemModel;

namespace docks {

class LayerAclMenu final : public QMenu {
	Q_OBJECT
public:
	explicit LayerAclMenu(QWidget *parent = nullptr);

	void setUserList(QAbstractItemModel *model);
	void setAcl(
		bool contentLock, bool propsLock, bool moveLock, int tier,
		const QVector<uint8_t> exclusive);
	void setCensored(bool censor);
	void setAlphaLock(bool alphaLock);
	void setAlphaLockEnabled(bool alphaLockEnabled);
	void setCanEdit(bool canEdit);

signals:
	void layerAlphaLockChange(bool alphaLock);
	void layerContentLockChange(bool contentLock);
	void layerPropsLockChange(bool propsLock);
	void layerMoveLockChange(bool moveLock);
	void layerLockAllChange(bool lockAll);
	void layerAccessTierChange(int tier);
	void layerUserAccessChanged(int userId, bool access);
	void layerCensoredChange(bool censor);

protected:
	void showEvent(QShowEvent *e) override;

private:
	void lockTriggered(bool checked);

	QAbstractItemModel *m_userlist;
	QVector<uint8_t> m_exclusive;
	QAction *m_alphaLock;
	QAction *m_censored;
	QAction *m_lockAll;
	QAction *m_contentLock;
	QAction *m_propsLock;
	QAction *m_moveLock;
	QActionGroup *m_tiers;
	QActionGroup *m_users;
	bool m_allUsersLocked = false;
	bool m_canEdit = true;
};

}

#endif // LAYERACLMENU_H
