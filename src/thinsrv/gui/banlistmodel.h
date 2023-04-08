// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef BANLISTMODEL_H
#define BANLISTMODEL_H

#include "thinsrv/gui/jsonlistmodel.h"

namespace server {
namespace gui {

class BanListModel final : public JsonListModel
{
	Q_OBJECT
public:
	explicit BanListModel(QObject *parent=nullptr);

	void addBanEntry(const QJsonObject &entry);
	void removeBanEntry(int id);

	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

protected:
	QVariant getData(const QString &key, const QJsonObject &obj) const override;
};

}
}

#endif
