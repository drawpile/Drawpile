/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "passwordstore.h"
#include "../libshared/util/paths.h"

#include <QFileInfo>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardItemModel>

PasswordStore::PasswordStore()
	: PasswordStore(utils::paths::writablePath("passwords.json"))
{
}

PasswordStore::PasswordStore(const QString &path)
	: m_passwordFilePath(path)
{
}

static PasswordStore::PasswordMap readPasswords(const QJsonObject &servers)
{
	PasswordStore::PasswordMap passwds;
	for(auto i=servers.constBegin();i!=servers.constEnd();++i) {
		const QJsonObject usermap = i.value().toObject();
		PasswordStore::PasswordEntries entries;
		for(auto j=usermap.constBegin();j!=usermap.constEnd();++j) {
			entries[j.key()] = j.value().toString();
		}
		passwds[i.key()] = entries;
	}
	return passwds;
}

static QJsonObject writePasswords(const PasswordStore::PasswordMap &servers)
{
	QJsonObject o;

	for(auto i=servers.constBegin();i!=servers.constEnd();++i) {
		const PasswordStore::PasswordEntries entries = i.value();
		QJsonObject server;
		for(auto j=entries.constBegin();j!=entries.constEnd();++j) {
			server[j.key()] = j.value();
		}
		o[i.key()] = server;
	}
	return o;
}

QJsonDocument PasswordStore::toJsonDocument() const
{
	QJsonObject root;
	root["server"] = writePasswords(m_serverPasswords);
	root["extauth"] = writePasswords(m_extauthPasswords);
	return QJsonDocument(root);
}

bool PasswordStore::load(QString *errorMessage)
{
	QFile f(m_passwordFilePath);
	if(!f.open(QFile::ReadOnly)) {
		if(errorMessage)
			*errorMessage = f.errorString();
		return false;
	}

	const QByteArray data = f.readAll();
	f.close();

	QJsonParseError jsonError;
	const QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);

	if(jsonError.error != QJsonParseError::NoError) {
		if(errorMessage)
			*errorMessage = jsonError.errorString();
		return false;
	}

	const QJsonObject root = doc.object();

	m_serverPasswords = readPasswords(root.value("server").toObject());
	m_extauthPasswords = readPasswords(root.value("extauth").toObject());

	return true;
}

bool PasswordStore::save(QString *errorMessage) const
{
	return saveAs(m_passwordFilePath, errorMessage);
}

bool PasswordStore::saveAs(const QString &path, QString *errorMessage) const
{
	QSaveFile f(path);
	if(!f.open(QFile::WriteOnly)) {
		if(errorMessage)
			*errorMessage = f.errorString();
		return false;
	}

	const QByteArray data = toJsonDocument().toJson();

	const qint64 written = f.write(data);
	if(written != data.length()) {
		if(errorMessage)
			*errorMessage = f.errorString();
		return false;
	}

	if(!f.commit()) {
		if(errorMessage)
			*errorMessage = f.errorString();
		return false;
	}

	return true;
}

QString PasswordStore::getPassword(const QString &server, const QString &username, Type type) const
{
	const PasswordMap map = (type == Type::Extauth) ? m_extauthPasswords : m_serverPasswords;

	if(!map.contains(server))
		return QString();

	const PasswordEntries entries = map[server];

	return entries[username.toLower()];
}

void PasswordStore::setPassword(const QString &server, const QString &username, Type type, const QString &password)
{
	PasswordMap &map = (type == Type::Extauth) ? m_extauthPasswords : m_serverPasswords;
	map[server][username.toLower()] = password;
}

bool PasswordStore::forgetPassword(const QString &server, const QString &username, Type type)
{
	PasswordMap &map = (type == Type::Extauth) ? m_extauthPasswords : m_serverPasswords;
	if(!map.contains(server))
		return false;
	PasswordEntries &entries = map[server];
	return entries.remove(username.toLower()) > 0;
}

void passwordMapToStandardItem(QStandardItem *parent, const PasswordStore::PasswordMap &map, int type)
{
	for(auto i=map.constBegin();i!=map.constEnd();++i) {
		const PasswordStore::PasswordEntries entries = i.value();
		if(!entries.isEmpty()) {
			QStandardItem *serverItem = new QStandardItem(i.key());

			for(auto j=entries.constBegin();j!=entries.constEnd();++j) {
				QStandardItem *passwordItem = new QStandardItem(j.key());
				passwordItem->setData(i.key(), Qt::UserRole + 1);
				passwordItem->setData(j.key(), Qt::UserRole + 2);
				passwordItem->setData(type, Qt::UserRole + 3);
				serverItem->appendRow(passwordItem);
			}

			parent->appendRow(serverItem);
		}
	}
}

QStandardItemModel *PasswordStore::toStandardItemModel(QObject *parent) const
{
	QStandardItemModel *model = new QStandardItemModel(parent);

	QStandardItem *server = new QStandardItem(QObject::tr("Servers"));
	passwordMapToStandardItem(server, m_serverPasswords, int(Type::Server));
	model->appendRow(server);

	QStandardItem *extauth = new QStandardItem(QObject::tr("Websites"));
	passwordMapToStandardItem(extauth, m_extauthPasswords, int(Type::Extauth));
	model->appendRow(extauth);

	return model;
}
