// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/strings.h"
#include <QCoreApplication>
#include <QLocale>

namespace strings {

QString percent()
{
	//: Percent, put after numbers as a unit, like 42%. Unless your language
	//: uses a different symbol or something, leave this as it is.
	return QCoreApplication::translate("Units", "%");
}

QString px()
{
	//: Abbreviation for pixels, put after a number as a unit, like 42px. Unless
	//: your language calls pixels something different, leave this as it is.
	return QCoreApplication::translate("Units", "px");
}

QString formatFileSize(qint64 sizeInBytes)
{
	qint64 mib = 1024LL * 1024LL;
	qint64 gib = 1024LL * mib;
	if(sizeInBytes >= gib) {
		return QStringLiteral("%1 GB").arg(
			QLocale().toString(qreal(sizeInBytes) / qreal(gib), 'f', 2));
	} else {
		return QStringLiteral("%1 MB").arg(
			QLocale().toString(qreal(sizeInBytes) / qreal(mib), 'f', 2));
	}
}

}
