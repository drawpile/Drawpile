// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/net/authlistmodel.h"
#include "libshared/util/qtcompat.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>

namespace net {

AuthListModel::AuthListModel(QObject *parent)
	: QAbstractTableModel(parent)
{
}

int AuthListModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_isOperator ? m_list.size() : 0;
}

int AuthListModel::columnCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : 2;
}

QVariant AuthListModel::data(const QModelIndex &index, int role) const
{
	if(m_isOperator && index.isValid() && index.row() >= 0 &&
	   index.row() < m_list.size()) {
		if(role == Qt::DisplayRole || role == Qt::ToolTipRole) {
			const AuthListEntry &e = m_list.at(index.row());
			switch(index.column()) {
			case 0:
				return e.username;
			case 1: {
				QStringList roles;
				if(e.mod) {
					roles.append(tr("Moderator"));
				}
				if(e.op) {
					roles.append(tr("Operator"));
				}
				if(e.trusted) {
					roles.append(tr("Trusted"));
				}
				return QLocale().createSeparatedList(roles);
			}
			}
		} else if(role == AuthIdRole) {
			return m_list.at(index.row()).authId;
		} else if(role == IsOpRole) {
			return m_list.at(index.row()).op;
		} else if(role == IsTrustedRole) {
			return m_list.at(index.row()).trusted;
		} else if(role == IsModRole) {
			return m_list.at(index.row()).mod;
		} else if(role == IsOwnRole) {
			return m_list.at(index.row()).authId == m_ownAuthId;
		}
	}
	return QVariant();
}

QVariant AuthListModel::headerData(
	int section, Qt::Orientation orientation, int role) const
{
	if(role == Qt::DisplayRole && orientation == Qt::Horizontal) {
		switch(section) {
		case 0:
			return tr("User");
		case 1:
			return tr("Roles");
		}
	}
	return QVariant();
}

void AuthListModel::update(const QJsonArray &auth)
{
	QSet<QString> authIds;
	for(const QJsonValue &value : auth) {
		QJsonObject o = value.toObject();
		net::AuthListEntry e = {
			o[QStringLiteral("authId")].toString(),
			o[QStringLiteral("username")].toString(),
			o[QStringLiteral("op")].toBool(),
			o[QStringLiteral("trusted")].toBool(),
			o[QStringLiteral("mod")].toBool(),
		};
		authIds.insert(e.authId);

		compat::sizetype i;
		compat::sizetype count = m_list.size();
		for(i = 0; i < count; ++i) {
			if(m_list[i].authId == e.authId) {
				break;
			}
		}

		if(count != 0 && i < count) {
			net::AuthListEntry &old = m_list[i];
			bool changed = old.username != e.username || old.op != e.op ||
						   old.trusted != e.trusted;
			if(changed) {
				old = e;
				QModelIndex idx = index(i, 0);
				dataChanged(idx, idx);
			}
		} else {
			beginInsertRows(QModelIndex(), count, count);
			m_list.append(e);
			endInsertRows();
		}
	}

	compat::sizetype i = 0;
	while(i < m_list.size()) {
		if(authIds.contains(m_list.at(i).authId)) {
			++i;
		} else {
			beginRemoveRows(QModelIndex(), i, i);
			m_list.removeAt(i);
			endRemoveRows();
		}
	}

	emit updateApplied();
}

void AuthListModel::clear()
{
	beginResetModel();
	m_list.clear();
	m_ownAuthId.clear();
	endResetModel();
}

void AuthListModel::setIsOperator(bool isOperator)
{
	if(m_isOperator != isOperator) {
		beginResetModel();
		m_isOperator = isOperator;
		endResetModel();
	}
}

void AuthListModel::setOwnAuthId(const QString &authId)
{
	if(m_ownAuthId != authId) {
		beginResetModel();
		m_ownAuthId = authId;
		endResetModel();
	}
}

}
