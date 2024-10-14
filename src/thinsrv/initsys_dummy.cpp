// SPDX-License-Identifier: GPL-3.0-or-later
#include "thinsrv/initsys.h"
#include <cstdio>

namespace initsys {

QString name()
{
	return QStringLiteral("dummy");
}

void notifyReady()
{
	// dummy
}

void notifyStatus(const QString &status)
{
#ifndef NDEBUG
	std::fprintf(stderr, "STATUS: %s\n", qUtf8Printable(status));
#else
	Q_UNUSED(status);
#endif
}

QList<int> getListenFds()
{
	return QList<int>();
}

}
