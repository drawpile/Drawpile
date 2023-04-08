// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SERVERLOGMODEL_H
#define SERVERLOGMODEL_H

#include "thinsrv/gui/jsonlistmodel.h"

namespace server {
namespace gui {

class ServerLogModel final : public JsonListModel
{
	Q_OBJECT
public:
	explicit ServerLogModel(QObject *parent=nullptr);

	//! Get the timestamp of the latest log entry
	QString lastTimestamp() const;

	void addLogEntry(const QJsonObject &entry);
	void addLogEntries(const QJsonArray &entries);

	QVariant data(const QModelIndex &index, int role) const override;

};

}
}

#endif

