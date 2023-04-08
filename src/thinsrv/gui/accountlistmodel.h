// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ACCOUNTLISTMODEL_H
#define ACCOUNTLISTMODEL_H

#include "thinsrv/gui/jsonlistmodel.h"

namespace server {
namespace gui {

class AccountListModel final : public JsonListModel
{
	Q_OBJECT
public:
	explicit AccountListModel(QObject *parent=nullptr);

	void addAccount(const QJsonObject &entry);
	void updateAccount(const QJsonObject &entry);
	void removeAccount(int id);

	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
};

}
}

#endif
