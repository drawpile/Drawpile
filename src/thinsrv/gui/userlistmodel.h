// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef USERLISTMODEL_H
#define USERLISTMODEL_H

#include "thinsrv/gui/jsonlistmodel.h"

namespace server {
namespace gui {

class UserListModel final : public JsonListModel
{
	Q_OBJECT
public:
	explicit UserListModel(QObject *parent=nullptr);

protected:
	QVariant getData(const QString &key, const QJsonObject &obj) const override;
	Qt::ItemFlags getFlags(const QJsonObject &obj) const override;
};

}
}

#endif
