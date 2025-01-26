// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/serverconfig.h"
#include <QJsonObject>
#include <QRegularExpression>

namespace server {

QString ServerConfig::getConfigString(ConfigKey key) const
{
	bool found = false;
	const QString val = getConfigValue(key, found);
	if(!found) {
		return key.defaultValue;
	}
	return val;
}

int ServerConfig::getConfigTime(ConfigKey key) const
{
	Q_ASSERT(key.type == ConfigKey::TIME);
	const QString val = getConfigString(key);

	const int t = parseTimeString(val);
	Q_ASSERT(t >= 0);
	return t;
}

int ServerConfig::getConfigSize(ConfigKey key) const
{
	Q_ASSERT(key.type == ConfigKey::SIZE);
	const QString val = getConfigString(key);

	const int s = parseSizeString(val);
	Q_ASSERT(s >= 0);
	return s;
}

int ServerConfig::getConfigInt(ConfigKey key) const
{
	Q_ASSERT(key.type == ConfigKey::INT);
	const QString val = getConfigString(key);
	bool ok;
	int i = val.toInt(&ok);
	Q_ASSERT(ok);
	return i;
}

bool ServerConfig::getConfigBool(ConfigKey key) const
{
	Q_ASSERT(key.type == ConfigKey::BOOL);
	const QString val = getConfigString(key).toLower();
	return val == QStringLiteral("1") || val == QStringLiteral("true");
}

QVariant ServerConfig::getConfigVariant(ConfigKey key) const
{
	switch(key.type) {
	case ConfigKey::STRING:
		return getConfigString(key);
	case ConfigKey::TIME:
		return getConfigTime(key);
	case ConfigKey::SIZE:
		return getConfigSize(key);
	case ConfigKey::INT:
		return getConfigInt(key);
	case ConfigKey::BOOL:
		return getConfigBool(key);
	}
	return QVariant(); // Shouldn't happen
}

bool ServerConfig::setConfigString(ConfigKey key, const QString &value)
{
	// Type specific sanity check
	switch(key.type) {
	case ConfigKey::STRING:
	case ConfigKey::BOOL:
		// no type specific validation for these
		break;
	case ConfigKey::SIZE:
		if(parseSizeString(value) < 0) {
			return false;
		}
		break;
	case ConfigKey::TIME:
		if(parseTimeString(value) < 0) {
			return false;
		}
		break;
	case ConfigKey::INT: {
		bool ok;
		value.toInt(&ok);
		if(!ok) {
			return false;
		}
		break;
	}
	}

	// TODO key specific validation

	setConfigValue(key, value);
	emit configValueChanged(key);
	return true;
}

void ServerConfig::setConfigInt(ConfigKey key, int value)
{
	Q_ASSERT(
		key.type == ConfigKey::INT || key.type == ConfigKey::SIZE ||
		key.type == ConfigKey::TIME);
	setConfigString(key, QString::number(value));
}

void ServerConfig::setConfigBool(ConfigKey key, bool value)
{
	Q_ASSERT(key.type == ConfigKey::BOOL);
	setConfigString(
		key, value ? QStringLiteral("true") : QStringLiteral("false"));
}

bool ServerConfig::setExternalBanEnabled(int id, bool enabled)
{
	if(enabled) {
		m_disabledExtBanIds.remove(id);
	} else {
		m_disabledExtBanIds.insert(id);
	}
	return true;
}

QJsonArray ServerConfig::getExternalBans() const
{
	QJsonArray bans;
	for(const ExtBan &ban : m_extBans) {
		bans.append(QJsonObject{
			{QStringLiteral("id"), ban.id},
			{QStringLiteral("ips"), banIpRangesToJson(ban.ips, true)},
			{QStringLiteral("ipsexcluded"),
			 banIpRangesToJson(ban.ipsExcluded, false)},
			{QStringLiteral("system"), banSystemToJson(ban.system)},
			{QStringLiteral("users"), banUsersToJson(ban.users)},
			{QStringLiteral("expires"), formatDateTime(ban.expires)},
			{QStringLiteral("comment"), ban.comment},
			{QStringLiteral("reason"), ban.reason},
			{QStringLiteral("enabled"), !m_disabledExtBanIds.contains(ban.id)},
		});
	}
	return bans;
}

bool ServerConfig::isAllowedAnnouncementUrl(const QUrl &url) const
{
	Q_UNUSED(url);
	return true;
}

BanResult ServerConfig::isAddressBanned(const QHostAddress &addr) const
{
	QDateTime now = QDateTime::currentDateTime();
	for(const ExtBan &ban : m_extBans) {
		BanReaction reaction = BanReaction::NotBanned;
		bool banned = !m_disabledExtBanIds.contains(ban.id) &&
					  ban.expires > now &&
					  isInAnyRange(addr, ban.ips, &reaction) &&
					  !isInAnyRange(addr, ban.ipsExcluded);
		if(banned) {
			return makeBanResult(
				ban, addr.toString(), QStringLiteral("IP"), reaction, true);
		}
	}
	return BanResult::notBanned();
}

BanResult ServerConfig::isSystemBanned(const QString &sid) const
{
	QDateTime now = QDateTime::currentDateTime();
	for(const ExtBan &ban : m_extBans) {
		BanReaction reaction = BanReaction::NotBanned;
		bool banned = !m_disabledExtBanIds.contains(ban.id) &&
					  ban.expires > now &&
					  isInAnySystem(sid, ban.system, reaction);
		if(banned) {
			return makeBanResult(
				ban, sid, QStringLiteral("SID"), reaction, false);
		}
	}
	return BanResult::notBanned();
}

BanResult ServerConfig::isUserBanned(long long userId) const
{
	QDateTime now = QDateTime::currentDateTime();
	for(const ExtBan &ban : m_extBans) {
		BanReaction reaction = BanReaction::NotBanned;
		bool banned = !m_disabledExtBanIds.contains(ban.id) &&
					  ban.expires > now &&
					  isInAnyUser(userId, ban.users, reaction);
		if(banned) {
			return makeBanResult(
				ban, QString::number(userId), QStringLiteral("User"), reaction,
				false);
		}
	}
	return BanResult::notBanned();
}

RegisteredUser ServerConfig::getUserAccount(
	const QString &username, const QString &password) const
{
	Q_UNUSED(password);
	return RegisteredUser{
		RegisteredUser::NotFound, username, QStringList(), nullptr};
}

bool ServerConfig::hasAnyUserAccounts() const
{
	return false;
}

int ServerConfig::parseTimeString(const QString &str)
{
	static const QRegularExpression re(
		QStringLiteral("\\A(\\d+(?:\\.\\d+)?)\\s*([dhms]?)\\z"));
	QRegularExpressionMatch m = re.match(str.toLower());
	if(!m.hasMatch()) {
		return -1;
	}

	float t = m.captured(1).toFloat();
	if(m.captured(2) == QStringLiteral("d")) {
		t *= 24 * 60 * 60;
	} else if(m.captured(2) == QStringLiteral("h")) {
		t *= 60 * 60;
	} else if(m.captured(2) == QStringLiteral("m")) {
		t *= 60;
	}

	return t;
}

int ServerConfig::parseSizeString(const QString &str)
{
	static const QRegularExpression re(
		QStringLiteral("\\A(\\d+(?:\\.\\d+)?)\\s*(gb|mb|kb|b)?\\z"));
	QRegularExpressionMatch m = re.match(str.toLower());
	if(!m.hasMatch()) {
		return -1;
	}

	float s = m.captured(1).toFloat();
	if(m.captured(2) == QStringLiteral("gb")) {
		s *= 1024 * 1024 * 1024;
	} else if(m.captured(2) == QStringLiteral("mb")) {
		s *= 1024 * 1024;
	} else if(m.captured(2) == QStringLiteral("kb")) {
		s *= 1024;
	}

	return s;
}

QDateTime ServerConfig::parseDateTime(const QString &expires)
{
	return QDateTime::fromString(
		expires, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QString ServerConfig::formatDateTime(const QDateTime &expires)
{
	return expires.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

bool ServerConfig::matchesBannedAddress(
	const QHostAddress &addr, const QHostAddress &ip, int subnet)
{
	switch(ip.protocol()) {
	case QAbstractSocket::IPv4Protocol: {
		bool ok;
		quint32 addrIpV4 = addr.toIPv4Address(&ok);
		return ok &&
			   QHostAddress(addrIpV4).isInSubnet(ip, subnet == 0 ? 32 : subnet);
	}
	case QAbstractSocket::IPv6Protocol:
		return QHostAddress(addr.toIPv6Address())
			.isInSubnet(ip, subnet == 0 ? 128 : subnet);
	default:
		return addr.isInSubnet(ip, subnet);
	}
}

BanReaction ServerConfig::parseReaction(const QString &reaction)
{
	if(reaction.isEmpty() || reaction == QStringLiteral("normal")) {
		return BanReaction::NormalBan;
	} else if(reaction == QStringLiteral("neterror")) {
		return BanReaction::NetError;
	} else if(reaction == QStringLiteral("garbage")) {
		return BanReaction::Garbage;
	} else if(reaction == QStringLiteral("hang")) {
		return BanReaction::Hang;
	} else if(reaction == QStringLiteral("timer")) {
		return BanReaction::Timer;
	} else {
		return BanReaction::Unknown;
	}
}

bool ServerConfig::isAddressInRange(
	const QHostAddress &addr, const QHostAddress &from, const QHostAddress &to)
{
	return !addr.isNull() && !from.isNull() && !to.isNull() &&
		   (isAddressInRangeV4(addr, from, to) ||
			isAddressInRangeV6(addr, from, to));
}

bool ServerConfig::isInAnyRange(
	const QHostAddress &addr, const QVector<BanIpRange> &ranges,
	BanReaction *outReaction)
{
	for(const BanIpRange &range : ranges) {
		if(isAddressInRange(addr, range.from, range.to)) {
			if(outReaction) {
				*outReaction = range.reaction;
			}
			return true;
		}
	}
	return false;
}

bool ServerConfig::isAddressInRangeV4(
	const QHostAddress &addr, const QHostAddress &from, const QHostAddress &to)
{
	bool ok;
	quint32 a4 = addr.toIPv4Address(&ok);
	if(ok) {
		quint32 f4 = from.toIPv4Address(&ok);
		if(ok && a4 >= f4) {
			quint32 t4 = to.toIPv4Address(&ok);
			return ok && a4 <= t4;
		}
	}
	return false;
}

bool ServerConfig::isAddressInRangeV6(
	const QHostAddress &addr, const QHostAddress &from, const QHostAddress &to)
{
	constexpr size_t SIZE = sizeof(Q_IPV6ADDR::c);
	static_assert(SIZE == 16, "IPv6 array is 16 bytes");
	Q_IPV6ADDR a6 = addr.toIPv6Address();
	Q_IPV6ADDR f6 = from.toIPv6Address();
	if(memcmp(a6.c, f6.c, SIZE) >= 0) {
		Q_IPV6ADDR t6 = to.toIPv6Address();
		return memcmp(a6.c, t6.c, SIZE) <= 0;
	}
	return false;
}

bool ServerConfig::isInAnySystem(
	const QString &sid, const QVector<BanSystemIdentifier> &system,
	BanReaction &outReaction)
{
	for(const BanSystemIdentifier &s : system) {
		if(s.sids.contains(sid)) {
			outReaction = s.reaction;
			return true;
		}
	}
	return false;
}

bool ServerConfig::isInAnyUser(
	long long userId, const QVector<BanUser> &users, BanReaction &outReaction)
{
	for(const BanUser &u : users) {
		if(u.ids.contains(userId)) {
			outReaction = u.reaction;
			return true;
		}
	}
	return false;
}

BanResult ServerConfig::makeBanResult(
	const ExtBan &ban, const QString &cause, const QString &sourceType,
	BanReaction reaction, bool isExemptable)
{
	// If we don't have a sensible reaction, treat this as a normal ban.
	if(reaction == BanReaction::NotBanned || reaction == BanReaction::Unknown) {
		reaction = BanReaction::NormalBan;
	}
	return {
		reaction,	ban.reason, ban.expires, cause, QStringLiteral("extban"),
		sourceType, ban.id,		isExemptable};
}

QJsonArray ServerConfig::banIpRangesToJson(
	const QVector<BanIpRange> &ranges, bool includeReaction)
{
	QJsonArray json;
	for(const BanIpRange &range : ranges) {
		QJsonObject object{
			{QStringLiteral("from"), range.from.toString()},
			{QStringLiteral("to"), range.to.toString()},
		};
		if(includeReaction) {
			object[QStringLiteral("reaction")] =
				reactionToString(range.reaction);
		}
		json.append(object);
	}
	return json;
}

QJsonArray
ServerConfig::banSystemToJson(const QVector<BanSystemIdentifier> &system)
{
	QJsonArray json;
	for(const BanSystemIdentifier &s : system) {
		QJsonArray sids;
		for(const QString &sid : s.sids) {
			sids.append(sid);
		}
		json.append(QJsonObject{
			{QStringLiteral("sids"), sids},
			{QStringLiteral("reaction"), reactionToString(s.reaction)},
		});
	}
	return json;
}

QJsonArray ServerConfig::banUsersToJson(const QVector<BanUser> &users)
{
	QJsonArray json;
	for(const BanUser &u : users) {
		QJsonArray ids;
		for(long long id : u.ids) {
			ids.append(id);
		}
		json.append(QJsonObject{
			{QStringLiteral("ids"), ids},
			{QStringLiteral("reaction"), reactionToString(u.reaction)},
		});
	}
	return json;
}

QString ServerConfig::reactionToString(BanReaction reaction)
{
	switch(reaction) {
	case BanReaction::NotBanned:
		return QStringLiteral("notbanned");
	case BanReaction::Unknown:
		return QStringLiteral("unknown");
	case BanReaction::NormalBan:
		return QStringLiteral("normal");
	case BanReaction::NetError:
		return QStringLiteral("neterror");
	case BanReaction::Garbage:
		return QStringLiteral("garbage");
	case BanReaction::Hang:
		return QStringLiteral("hang");
	case BanReaction::Timer:
		return QStringLiteral("timer");
	}
	return QString();
}

}
