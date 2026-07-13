// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBSHARED_UTIL_PATHINFO
#define LIBSHARED_UTIL_PATHINFO
#include <QFileInfo>
#include <QString>

namespace utils {

// Kind of like QFileInfo, except that it understands Android content URIs and
// doesn't use weird terminology in its method names.
class PathInfo final {
public:
	explicit PathInfo(const QString &path = QString());

	const QString &path() const { return m_path; }
	void setPath(const QString &path);
	void clear();

#ifdef Q_OS_ANDROID
	bool isContentUri() const { return type() == TYPE_CONTENT_URI; }
#else
	constexpr bool isContentUri() { return false; }
#endif

	// Returns the absolute path, like QFileInfo::absoluteFilePath. For Android
	// content URIs, this is just the input string itself unmodified.
	QString abspath() const;

	// Returns the last component of this path, like QFileInfo::fileName. For
	// Android content URIs, this tries to retrieve the display name and punts
	// to the last path segment if it can't do that. If that also fails it may
	// end up just returning the entire input string.
	QString basename() const;

	// Returns the basename with the last dot and anything after removed. If the
	// dot is at the beginning or there is no dot, it returns the entire string.
	QString basenameWithoutExtension() const
	{
		return stripExtension(basename());
	}

	static QString stripExtension(const QString &s);

	// Returns the opposite side of the dot than basenameWithoutExtension.
	QString extension() const { return extractExtension(basename()); }

	static QString extractExtension(const QString &s);

private:
	static constexpr unsigned int TYPE_EMPTY = 0u;
	static constexpr unsigned int TYPE_PATH = 1u;
#ifdef Q_OS_ANDROID
	static constexpr unsigned int TYPE_CONTENT_URI = 2u;
#endif
	static constexpr unsigned int FLAG_TYPE_MASK = 0x3u;
	static constexpr unsigned int FLAG_HAVE_FILE_INFO = (1u << 2u);
#ifdef Q_OS_ANDROID
	static constexpr unsigned int FLAG_HAVE_CONTENT_URI_BASENAME = (1u << 3u);
#endif

	void initFlags();
	unsigned int type() const { return m_flags & FLAG_TYPE_MASK; }

	QFileInfo &fileInfo() const;

	QString m_path;
	mutable QFileInfo m_fileInfo;
#ifdef Q_OS_ANDROID
	mutable QString m_contentUriBasename;
#endif
	mutable unsigned int m_flags;
};

}

#endif
