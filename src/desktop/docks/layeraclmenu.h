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
	void layerAclChange(bool lock, DP_AccessTier tier, QVector<uint8_t> ids);

	/**
	 * @brief The censored checkbox was toggled
	 */
	void layerCensoredChange(bool censor);

protected:
	void showEvent(QShowEvent *e) override;

private slots:
	void userClicked(QAction *useraction);

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
