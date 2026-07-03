// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/tempfile.h"
#include <QFile>
#include <QTemporaryFile>

namespace utils {

TempFile::~TempFile()
{
	removePath();
}

bool TempFile::setTemporaryPath()
{
	QTemporaryFile tempFile;
	if(tempFile.open()) {
		tempFile.setAutoRemove(false);
		tempFile.close();
		setPath(tempFile.fileName());
		return true;
	} else {
		qWarning(
			"Error %d creating temporary file: %s", int(tempFile.error()),
			qUtf8Printable(tempFile.errorString()));
		return false;
	}
}

void TempFile::setPath(const QString &path)
{
	if(m_path != path) {
		removePath();
		m_path = path;
	}
}

void TempFile::removePath()
{
	if(m_autoRemove && !m_path.isEmpty()) {
		QFile f(m_path);
		if(!f.remove()) {
			qWarning(
				"Error %d removing temporary file '%s': %s", int(f.error()),
				qUtf8Printable(m_path), qUtf8Printable(f.errorString()));
		}
	}
}

}
