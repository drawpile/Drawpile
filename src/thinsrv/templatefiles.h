/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#ifndef DP_SERVER_TEMPLATEFILES_H
#define DP_SERVER_TEMPLATEFILES_H

#include "libserver/templateloader.h"

#include <QObject>
#include <QDir>
#include <QJsonObject>
#include <QDateTime>
#include <QHash>

class QFileSystemWatcher;

namespace server {

class TemplateFiles final : public QObject, public TemplateLoader {
public:
	explicit TemplateFiles(const QDir &dir, QObject *parent=nullptr);

	QJsonArray templateDescriptions() const override;
	QJsonObject templateDescription(const QString &alias) const override;
	bool exists(const QString &alias) const override;

	bool init(SessionHistory *session) const override;

private slots:
	void scanDirectory();

private:
	QJsonObject templateFileDescription(const QString &path, const QString &alias) const;

	struct Template {
		QJsonObject description;
		QString filename;
		QDateTime lastmod;
	};

	QHash<QString,Template> m_templates;
	QFileSystemWatcher *m_watcher;
	QDir m_dir;
};

}

#endif

