// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/pathinfo.h"
#ifdef Q_OS_ANDROID
#	include "libclient/utils/androidutils.h"
#endif

namespace utils {

PathInfo::PathInfo(const QString &path)
	: m_path(path)
{
	initFlags();
}

void PathInfo::setPath(const QString &path)
{
	if(path != m_path) {
		m_path = path;
		initFlags();
	}
}

void PathInfo::clear()
{
	setPath(QString());
}

QString PathInfo::abspath() const
{
	switch(type()) {
	case TYPE_EMPTY:
		return QString();
	case TYPE_PATH:
		return fileInfo().absoluteFilePath();
#ifdef Q_OS_ANDROID
	case TYPE_CONTENT_URI:
		return m_path;
#endif
	}
	Q_UNREACHABLE();
}

QString PathInfo::basename() const
{
	switch(type()) {
	case TYPE_EMPTY:
		return QString();
	case TYPE_PATH:
		return fileInfo().fileName();
#ifdef Q_OS_ANDROID
	case TYPE_CONTENT_URI:
		if(!(m_flags & FLAG_HAVE_CONTENT_URI_BASENAME)) {
			m_flags |= FLAG_HAVE_CONTENT_URI_BASENAME;
			m_contentUriBasename = utils::androidGetContentUriBasename(m_path);
		}
		return m_contentUriBasename;
#endif
	}
	Q_UNREACHABLE();
}

QString PathInfo::stripExtension(const QString &s)
{
	int i = s.lastIndexOf(QChar('.'));
	if(i > 0) {
		return s.mid(0, i);
	} else {
		return s;
	}
}

QString PathInfo::extractExtension(const QString &s)
{
	int i = s.lastIndexOf(QChar('.'));
	if(i > 0) {
		return s.mid(i + 1);
	} else {
		return QString();
	}
}

void PathInfo::initFlags()
{
	if(m_path.isEmpty()) {
		m_flags = TYPE_EMPTY;
#ifdef Q_OS_ANDROID
	} else if(
		m_path.startsWith(QStringLiteral("content://"), Qt::CaseInsensitive)) {
		m_flags = TYPE_CONTENT_URI;
#endif
	} else {
		m_flags = TYPE_PATH;
	}
}

QFileInfo &PathInfo::fileInfo() const
{
	if(!(m_flags & FLAG_HAVE_FILE_INFO)) {
		m_flags |= FLAG_HAVE_FILE_INFO;
		m_fileInfo = QFileInfo(m_path);
	}
	return m_fileInfo;
}

}
