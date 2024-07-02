// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include "dpcommon/input.h"
#include "dpengine/player.h"
#include "parson.h"
}
#include "libserver/sessionhistory.h"
#include "libshared/net/protover.h"
#include "libshared/util/validators.h"
#include "thinsrv/templatefiles.h"
#include <QFileSystemWatcher>
#include <QJsonArray>

namespace server {

TemplateFiles::TemplateFiles(const QDir &dir, QObject *parent)
	: QObject(parent)
	, m_dir(dir)
{
	m_dir.setNameFilters({"*.dprec", "*.dptxt", "*.dprec.*", "*.dptxt.*"});
	m_watcher =
		new QFileSystemWatcher(QStringList() << dir.absolutePath(), this);
	connect(
		m_watcher, &QFileSystemWatcher::directoryChanged, this,
		&TemplateFiles::scanDirectory);

	scanDirectory();
}

void TemplateFiles::scanDirectory()
{
	qDebug(
		"%s: scanning template directory...", qPrintable(m_dir.absolutePath()));

	QHash<QString, Template> templates;

	for(const QFileInfo &f : m_dir.entryInfoList()) {
		const QString alias = f.baseName();
		if(validateSessionIdAlias(alias)) {
			if(m_templates.contains(alias) &&
			   m_templates[alias].lastmod == f.lastModified()) {
				// Template is unchanged
				templates[alias] = m_templates[alias];

			} else {
				// Template has been modified or added
				templates[alias] = Template{
					templateFileDescription(f.absoluteFilePath(), alias),
					f.absoluteFilePath(), f.lastModified()};
				qDebug(
					"%s: template updated", qPrintable(f.absoluteFilePath()));
			}
		}
	}

	m_templates = templates;
}

QJsonArray TemplateFiles::templateDescriptions() const
{
	QJsonArray a;
	for(const Template &t : m_templates) {
		if(!t.description.isEmpty())
			a << t.description;
	}

	return a;
}

QJsonObject TemplateFiles::templateDescription(const QString &alias) const
{
	if(m_templates.contains(alias))
		return m_templates[alias].description;
	else
		return QJsonObject();
}

static QString getHeaderString(JSON_Object *header, const char *key)
{
	JSON_Value *value = json_object_get_value(header, key);
	if(value && json_value_get_type(value) == JSONString) {
		return QString::fromUtf8(json_value_get_string(value));
	}
	return QString{};
}

static int getHeaderInt(JSON_Object *header, const char *key)
{
	JSON_Value *value = json_object_get_value(header, key);
	if(value) {
		switch(json_value_get_type(value)) {
		case JSONString:
			return QString::fromUtf8(json_value_get_string(value)).toInt();
		case JSONNumber:
			return int(json_value_get_number(value));
		default:
			break;
		}
	}
	return 0;
}

static bool getHeaderBool(JSON_Object *header, const char *key)
{
	JSON_Value *value = json_object_get_value(header, key);
	if(value) {
		switch(json_value_get_type(value)) {
		case JSONString: {
			QString s = QString::fromUtf8(json_value_get_string(value));
			return !s.isEmpty() &&
				   QString::compare(s, "false", Qt::CaseInsensitive) != 0;
		}
		case JSONNumber:
			return json_value_get_number(value) != 0.0;
		case JSONBoolean:
			return json_value_get_boolean(value);
		default:
			break;
		}
	}
	return false;
}

static int getHeaderMaxUserCount(JSON_Object *header)
{
	int maxUserCount = getHeaderInt(header, "maxUserCount");
	return maxUserCount <= 0 ? 25 : qBound(1, maxUserCount, 255);
}

QJsonObject TemplateFiles::templateFileDescription(
	const QString &path, const QString &alias) const
{
	DP_Input *input = DP_file_input_new_from_path(qUtf8Printable(path));
	DP_Player *player =
		input ? DP_player_new(DP_PLAYER_TYPE_GUESS, nullptr, input, nullptr)
			  : nullptr;
	if(!player) {
		qWarning(
			"Error loading template '%s': %s", qUtf8Printable(path),
			DP_error());
		return QJsonObject{};
	} else if(!DP_player_compatible(player)) {
		qWarning("Incompatible recording '%s'", qUtf8Printable(path));
		DP_player_free(player);
		return QJsonObject{};
	}

	JSON_Value *header_value = DP_player_header(player);
	JSON_Object *header = json_value_get_object(header_value);
	QString founder = getHeaderString(header, "founder");
	QJsonObject desc{
		{"alias", alias},
		{"protocol", getHeaderString(header, "version")},
		{"maxUserCount", getHeaderMaxUserCount(header)},
		{"founder", founder.isEmpty() ? "-" : founder},
		{"hasPassword", !getHeaderString(header, "password").isEmpty()},
		{"title", getHeaderString(header, "title")},
		{"nsfm", getHeaderBool(header, "nsfm")},
	};

	DP_player_free(player);
	return desc;
}

bool TemplateFiles::exists(const QString &alias) const
{
	return m_templates.contains(alias) &&
		   !m_templates[alias].description.isEmpty();
}

bool TemplateFiles::init(SessionHistory *session) const
{
	if(!m_templates.contains(session->idAlias()))
		return false;

	QString path = m_templates[session->idAlias()].filename;
	DP_Input *input = DP_file_input_new_from_path(qUtf8Printable(path));
	DP_Player *player =
		input ? DP_player_new(DP_PLAYER_TYPE_GUESS, nullptr, input, nullptr)
			  : nullptr;
	if(!player) {
		qWarning(
			"Error loading template '%s': %s", qUtf8Printable(path),
			DP_error());
		return false;
	} else if(!DP_player_compatible(player)) {
		qWarning("Incompatible recording '%s'", qUtf8Printable(path));
		DP_player_free(player);
		return false;
	}

	JSON_Value *header_value = DP_player_header(player);
	JSON_Object *header = json_value_get_object(header_value);

	// Set session metadata
	Q_ASSERT(
		protocol::ProtocolVersion::fromString(
			getHeaderString(header, "version")) == session->protocolVersion());
	session->setMaxUsers(getHeaderMaxUserCount(header));
	session->setPasswordHash(getHeaderString(header, "password").toUtf8());
	session->setOpwordHash(getHeaderString(header, "opword").toUtf8());
	session->setTitle(getHeaderString(header, "title"));

	QString announce = getHeaderString(header, "announce");
	if(!announce.isEmpty()) {
		session->addAnnouncement(announce);
	}

	SessionHistory::Flags flags;
	if(getHeaderBool(header, "nsfm"))
		flags |= SessionHistory::Nsfm;
	if(getHeaderBool(header, "persistent"))
		flags |= SessionHistory::Persistent;
	if(getHeaderBool(header, "preserveChat"))
		flags |= SessionHistory::PreserveChat;
	if(getHeaderBool(header, "deputies"))
		flags |= SessionHistory::Deputies;
	if(getHeaderBool(header, "idleOverride"))
		flags |= SessionHistory::IdleOverride;
	if(getHeaderBool(header, "allowWeb"))
		flags |= SessionHistory::AllowWeb;
	session->setFlags(flags);

	// Set initial history
	bool keepReading = true;
	do {
		DP_Message *msg;
		DP_PlayerResult result = DP_player_step(player, &msg);
		switch(result) {
		case DP_PLAYER_SUCCESS:
			session->addMessage(net::Message::noinc(msg));
			break;
		case DP_PLAYER_ERROR_PARSE:
			qWarning(
				"Parse error in template %s: %s",
				qUtf8Printable(session->idAlias()), DP_error());
			break;
		case DP_PLAYER_RECORDING_END:
			keepReading = false;
			break;
		default:
			qWarning(
				"Error in template %s: %s", qUtf8Printable(session->idAlias()),
				DP_error());
			keepReading = false;
			break;
		}
	} while(keepReading);

	DP_player_free(player);
	return true;
}

}
