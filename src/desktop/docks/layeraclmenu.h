// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LAYERACLMENU_H
#define LAYERACLMENU_H

extern "C" {
#include <dpmsg/acl.h>
}

#include <QMenu>

class QAbstractItemModel;

namespace docks {

class LayerAclMenu final : public QMenu
{
    Q_OBJECT
public:
	explicit LayerAclMenu(QWidget *parent=nullptr);

	void setUserList(QAbstractItemModel *model);
	void setAcl(bool lock, int tier, const QVector<uint8_t> acl);
	void setCensored(bool censor);
	void setAlphaLock(bool alphaLock);
	void setAlphaLockEnabled(bool alphaLockEnabled);

signals:
	void layerLockChange(bool locked);
	void layerAlphaLockChange(bool alphaLock);
	void layerAccessTierChange(int tier);
	void layerUserAccessChanged(int userId, bool access);
	void layerCensoredChange(bool censor);

protected:
	void showEvent(QShowEvent *e) override;

private:
	void lockTriggered(bool checked);

	QAbstractItemModel *m_userlist;
	QVector<uint8_t> m_exclusives;
	QAction *m_lock;
	QAction *m_alphaLock;
	QAction *m_censored;
	QActionGroup *m_tiers;
	QActionGroup *m_users;
};

}

#endif // LAYERACLMENU_H
