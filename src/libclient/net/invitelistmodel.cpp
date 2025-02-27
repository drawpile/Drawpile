// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/net/invitelistmodel.h"
#include <QFont>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

namespace net {

InviteListModel::Use InviteListModel::Use::fromJson(const QJsonObject &json)
{
	return Use{
		json.value(QStringLiteral("name")).toString(),
		QDateTime::fromString(
			json.value(QStringLiteral("at")).toString(), Qt::ISODate)};
}

bool InviteListModel::Use::operator==(const Use &other) const
{
	return name == other.name && at == other.at;
}

bool InviteListModel::Use::operator<(const Use &other) const
{
	return at < other.at || (at == other.at && name < other.name);
}

InviteListModel::Invite
InviteListModel::Invite::fromJson(const QJsonObject &json)
{
	return Invite{
		json.value(QStringLiteral("secret")).toString(),
		json.value(QStringLiteral("creator")).toString(),
		QDateTime::fromString(
			json.value(QStringLiteral("at")).toString(), Qt::ISODate),
		usesFromJson(json.value(QStringLiteral("uses")).toArray()),
		json.value("maxUses").toInt(),
		json.value(QStringLiteral("op")).toBool(),
		json.value(QStringLiteral("trust")).toBool()};
}

QVector<InviteListModel::Use>
InviteListModel::Invite::usesFromJson(const QJsonArray &json)
{
	QVector<Use> uses;
	uses.reserve(json.size());
	for(const QJsonValue &value : json) {
		if(value.isObject()) {
			uses.append(Use::fromJson(value.toObject()));
		}
	}
	std::sort(uses.begin(), uses.end());
	return uses;
}

bool InviteListModel::Invite::operator<(const Invite &other) const
{
	return at < other.at || (at == other.at && secret < other.secret);
}


InviteListModel::InviteListModel(QObject *parent)
	: QAbstractItemModel(parent)
{
}

QModelIndex
InviteListModel::index(int row, int column, const QModelIndex &parent) const
{
	if(m_invites.isEmpty() && row == 0 && column >= 0 &&
	   column < int(Column::Count) && !parent.isValid()) {
		return createIndex(row, column, quintptr(0));
	} else if(isInBounds(row, column, parent)) {
		if(parent.isValid()) {
			return createIndex(row, column, quintptr(parent.row() + 1));
		} else {
			return createIndex(row, column, quintptr(0));
		}
	} else {
		return QModelIndex();
	}
}

QModelIndex InviteListModel::parent(const QModelIndex &child) const
{
	if(child.isValid() && child.internalId() > 0 &&
	   child.internalId() <= quintptr(m_invites.size())) {
		return createIndex(int(child.internalId() - 1), child.column());
	} else {
		return QModelIndex();
	}
}

int InviteListModel::rowCount(const QModelIndex &parent) const
{
	if(!parent.isValid()) {
		return m_invites.isEmpty() ? 1 : m_invites.size();
	} else {
		int row = parent.row();
		if(row >= 0 && row < m_invites.size()) {
			return m_invites[row].uses.size();
		} else {
			return 0;
		}
	}
}

int InviteListModel::columnCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 1 : int(Column::Count);
}

QVariant InviteListModel::data(const QModelIndex &index, int role) const
{
	if(m_invites.isEmpty()) {
		if(index.row() == 0 && index.column() == 0 && index.internalId() == 0) {
			switch(role) {
			case Qt::DisplayRole:
				return tr("No invite codes, click Create to add one.");
			case Qt::FontRole:
				QFont font;
				font.setItalic(true);
				return font;
			}
		}
	} else if(isIndexInBounds(index)) {
		quintptr i = index.internalId();
		if(i > 0) {
			const Invite &invite = m_invites[i - 1];
			switch(role) {
			case Qt::DisplayRole:
				if(index.column() == 0) {
					const Use &use = invite.uses[index.row()];
					//: %1 is a date and time, %2 is a username.
					return tr("Used %1 by %2")
						.arg(
							use.at.toString(
								QLocale().dateTimeFormat(QLocale::ShortFormat)),
							use.name);
				} else {
					break;
				}
			case SecretRole:
				return invite.secret;
			default:
				break;
			}
		} else {
			const Invite &invite = m_invites[index.row()];
			switch(role) {
			case Qt::DisplayRole:
			case Qt::ToolTipRole:
				switch(index.column()) {
				case int(Column::Secret):
					return invite.secret;
				case int(Column::Creator):
					if(role == Qt::ToolTipRole) {
						//: %1 is a date and time, %2 is a username
						return tr("Created %1 by %2")
							.arg(
								invite.at.toString(QLocale().dateTimeFormat()),
								invite.creator);
					} else {
						return invite.creator;
					}
				case int(Column::Role):
					if(invite.op) {
						if(role == Qt::ToolTipRole && invite.trust) {
							return tr("Operator and Trusted");
						} else {
							return tr("Operator");
						}
					} else if(invite.trust) {
						return tr("Trusted");
					} else {
						return QVariant();
					}
				case int(Column::Uses):
					//: This is the "uses" column in the invite code table. %1
					//: is how often the invite has been used, %2 is how many
					//: there are total.
					return tr("%1/%2")
						.arg(invite.uses.size())
						.arg(invite.maxUses);
				default:
					return QVariant();
				}
			case SecretRole:
				return invite.secret;
			default:
				break;
			}
		}
	}
	return QVariant();
}

QVariant InviteListModel::headerData(
	int section, Qt::Orientation orientation, int role) const
{
	if(orientation == Qt::Horizontal && role == Qt::DisplayRole) {
		switch(section) {
		case int(Column::Secret):
			return tr("Code");
		case int(Column::Creator):
			return tr("Creator");
		case int(Column::Role):
			return tr("Role");
		case int(Column::Uses):
			return tr("Uses");
		default:
			break;
		}
	}
	return QVariant();
}

Qt::ItemFlags InviteListModel::flags(const QModelIndex &index) const
{
	if(index.internalId() == 0) {
		if(m_invites.isEmpty()) {
			return {};
		} else {
			return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
		}
	} else {
		return Qt::ItemNeverHasChildren;
	}
}

QSize InviteListModel::span(const QModelIndex &index) const
{
	if(index.isValid() && ((m_invites.size() == 0 && index.internalId() == 0) ||
						   index.internalId() > 0)) {
		return QSize(int(Column::Count) - 1, 1);
	}
	return QSize(1, 1);
}

void InviteListModel::update(const QJsonArray json)
{
	beginResetModel();
	m_invites.clear();
	m_invites.reserve(json.size());
	for(const QJsonValue value : json) {
		if(value.isObject()) {
			m_invites.append(Invite::fromJson(value.toObject()));
		}
	}
	std::sort(m_invites.begin(), m_invites.end());
	endResetModel();
	emit invitesUpdated();
}

void InviteListModel::clear()
{
	beginResetModel();
	m_invites.clear();
	endResetModel();
}

QModelIndex InviteListModel::indexOfSecret(const QString &secret) const
{
	for(int i = 0, count = m_invites.size(); i < count; ++i) {
		if(m_invites[i].secret == secret) {
			return index(i, 0);
		}
	}
	return QModelIndex();
}

QStringList
InviteListModel::getSecretsInOrder(const QSet<QString> &secrets) const
{
	QStringList orderedSecrets;
	if(!secrets.isEmpty()) {
		orderedSecrets.reserve(secrets.size());
		for(const Invite &invite : m_invites) {
			if(secrets.contains(invite.secret)) {
				orderedSecrets.append(invite.secret);
			}
		}
	}
	return orderedSecrets;
}

bool InviteListModel::isInBounds(
	int row, int column, const QModelIndex &parent) const
{
	if(row >= 0) {
		if(parent.isValid()) {
			return column == 0 && parent.internalId() == 0 &&
				   parent.row() < m_invites.size() &&
				   row < m_invites[parent.row()].uses.size();
		} else {
			return column >= 0 && column < int(Column::Count) &&
				   row < m_invites.size();
		}
	} else {
		return false;
	}
}

bool InviteListModel::isIndexInBounds(const QModelIndex &index) const
{
	if(index.isValid()) {
		QModelIndex parent =
			index.internalId() > 0
				? createIndex(index.internalId() - 1, index.column())
				: QModelIndex();
		return isInBounds(index.row(), index.column(), parent);
	} else {
		return false;
	}
}

}
