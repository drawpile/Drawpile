// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_TEMPFILE_H
#define LIBCLIENT_UTILS_TEMPFILE_H
#include <QString>

namespace utils {

class TempFile final {
	Q_DISABLE_COPY_MOVE(TempFile)
public:
	TempFile() = default;
	~TempFile();

	const QString &path() const { return m_path; }

	// Sets a temporary path acquired from opening and closing a QTemporaryFile.
	// That file will exist as an empty file. If opening the file fails, returns
	// false and doesn't alter the path. If it succeeds, it will remove the
	// existing path if m_autoRemove is true.
	bool setTemporaryPath();

	// Sets the given path, if that is different from the current path and
	// m_autoRemove is true, it will remove the current path.
	void setPath(const QString &path);

	bool autoRemove() const { return m_autoRemove; }
	void setAutoRemove(bool autoRemove) { m_autoRemove = autoRemove; }

private:
	void removePath();

	QString m_path;
	bool m_autoRemove = true;
};

}

#endif
