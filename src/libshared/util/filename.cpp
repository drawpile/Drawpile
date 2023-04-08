// SPDX-License-Identifier: GPL-3.0-or-later

#include "libshared/util/filename.h"

#include <QFileInfo>
#include <QDir>

namespace utils {

QString uniqueFilename(const QDir &dir, const QString &name, const QString &extension, bool fullpath)
{
	QFileInfo f;
	int n=0;
	do {
		QString fullname;
		if(n==0)
			fullname = name + "." + extension;
		else
			fullname = QStringLiteral("%2 (%1).%3").arg(n).arg(name, extension);

		f.setFile(dir, fullname);
		++n;
	} while(f.exists());

	if(fullpath)
		return f.absoluteFilePath();
	else
		return f.fileName();
}

QString makeFilenameUnique(const QString &path, const QString &defaultExtension)
{
	Q_ASSERT(defaultExtension.at(0) == '.');

	const QFileInfo f(path);

	if(!f.exists())
		return path;

	const QString name = f.fileName();
	QString base, ext;

	int exti = name.lastIndexOf(defaultExtension, -1, Qt::CaseInsensitive);
	if(exti>0) {
		base = name.left(exti);
		ext = name.mid(exti+1);
	} else {
		base = name;
		ext = defaultExtension.mid(1);
	}

	return uniqueFilename(f.absoluteDir(), base, ext);
}

}
