// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/templatefiles.h"
#include "libshared/record/reader.h"
#include "libserver/sessionhistory.h"
#include "libshared/util/validators.h"

#include <QJsonArray>
#include <QFileSystemWatcher>

namespace server {

TemplateFiles::TemplateFiles(const QDir &dir, QObject *parent)
	: QObject(parent), m_dir(dir)
{
	m_dir.setNameFilters(QStringList() << "*.dprec" << "*.dptxt" << "*.dprecz" << "*.dptxtz" << "*.dprec.*" << "*.dptxt.*");
	m_watcher = new QFileSystemWatcher(QStringList() << dir.absolutePath(), this);
	connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &TemplateFiles::scanDirectory);

	scanDirectory();
}

void TemplateFiles::scanDirectory()
{
	qDebug("%s: scanning template directory...", qPrintable(m_dir.absolutePath()));

	QHash<QString, Template> templates;

	for(const QFileInfo &f : m_dir.entryInfoList()) {
		const QString alias = f.baseName();
		if(validateSessionIdAlias(alias)) {
			if(m_templates.contains(alias) && m_templates[alias].lastmod == f.lastModified()) {
				// Template is unchanged
				templates[alias] = m_templates[alias];

			} else {
				// Template has been modified or added
				templates[alias] = Template {
					templateFileDescription(f.absoluteFilePath(), alias),
					f.absoluteFilePath(),
					f.lastModified()
				};
				qDebug("%s: template updated", qPrintable(f.absoluteFilePath()));
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

QJsonObject TemplateFiles::templateFileDescription(const QString &path, const QString &alias) const
{
	recording::Reader reader(path);
	recording::Compatibility compat = reader.open();
	if(compat != recording::COMPATIBLE) {
		qWarning("%s: template not compatible", qPrintable(path));
		return QJsonObject();
	}

	QJsonObject desc;
	desc["alias"] = alias;
	desc["protocol"] = reader.metadata().value("version");
	desc["maxUserCount"] = reader.metadata().value("maxUserCount").toInt(25);
	desc["founder"] = reader.metadata().value("founder").toString("-");
	desc["hasPassword"] = !reader.metadata().value("password").toString().isEmpty();
	desc["title"] = reader.metadata().value("title");
	desc["nsfm"] = reader.metadata().value("nsfm").toBool(false);

	return desc;
}

bool TemplateFiles::exists(const QString &alias) const
{
	return m_templates.contains(alias) && !m_templates[alias].description.isEmpty();
}

bool TemplateFiles::init(SessionHistory *session) const
{
	if(!m_templates.contains(session->idAlias()))
		return false;

	recording::Reader reader(m_templates[session->idAlias()].filename);
	if(reader.open() != recording::COMPATIBLE) {
		qWarning("%s: template not compatible", qPrintable(m_templates[session->idAlias()].filename));
		return false;
	}

	// Set session metadata
	Q_ASSERT(protocol::ProtocolVersion::fromString(reader.metadata().value("version").toString()) == session->protocolVersion());
	session->setMaxUsers(reader.metadata().value("maxUserCount").toInt(25));
	session->setPasswordHash(reader.metadata().value("password").toString().toUtf8());
	session->setOpwordHash(reader.metadata().value("opword").toString().toUtf8());
	session->setTitle(reader.metadata().value("title").toString());

	if(reader.metadata().contains("announce")) {
		session->addAnnouncement(reader.metadata()["announce"].toString());
	}

	SessionHistory::Flags flags;
	if(reader.metadata().value("nsfm").toBool())
		flags |= SessionHistory::Nsfm;
	if(reader.metadata().value("persistent").toBool())
		flags |= SessionHistory::Persistent;
	if(reader.metadata().value("preserveChat").toBool())
		flags |= SessionHistory::PreserveChat;
	if(reader.metadata().value("deputies").toBool())
		flags |= SessionHistory::Deputies;
	session->setFlags(flags);

	// Set initial history
	bool keepReading=true;
	do {
		recording::MessageRecord r = reader.readNext();
		switch(r.status) {
		case recording::MessageRecord::OK:
			session->addMessage(protocol::MessagePtr::fromNullable(r.message));
			break;
		case recording::MessageRecord::INVALID:
			qWarning("%s: Invalid message (type %d, len %d) in template!", qPrintable(session->idAlias()), r.invalid_type, r.invalid_len);
			break;
		case recording::MessageRecord::END_OF_RECORDING:
			keepReading = false;
			break;
		}
	} while(keepReading);

	return true;
}

}

