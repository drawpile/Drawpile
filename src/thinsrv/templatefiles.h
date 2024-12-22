// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_SERVER_TEMPLATEFILES_H
#define DP_SERVER_TEMPLATEFILES_H
#include "libserver/templateloader.h"
#include <QDateTime>
#include <QDir>
#include <QHash>
#include <QObject>

class QFileSystemWatcher;

namespace server {

class TemplateFiles final : public QObject, public TemplateLoader {
public:
	explicit TemplateFiles(const QDir &dir, QObject *parent = nullptr);

	QVector<QJsonObject> templateDescriptions() const override;
	QJsonObject templateDescription(const QString &alias) const override;
	bool exists(const QString &alias) const override;

	bool init(SessionHistory *session) const override;

private slots:
	void scanDirectory();

private:
	QJsonObject
	templateFileDescription(const QString &path, const QString &alias) const;

	struct Template {
		QJsonObject description;
		QString filename;
		QDateTime lastmod;
	};

	QHash<QString, Template> m_templates;
	QFileSystemWatcher *m_watcher;
	QDir m_dir;
};

}

#endif
