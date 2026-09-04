#include "libclient/io/files.h"
#include "libshared/util/qtcompat.h"
#include <QCoreApplication>
#include <QFile>
#include <QSaveFile>
#include <cmake-config/config.h>

namespace io {

bool looksLikeCanvasReplacingSuffix(const QString &suffix)
{
	for(const char *ext : cmake_config::file_group::canvasReplacing()) {
		if(suffix.compare(QString::fromUtf8(ext), Qt::CaseInsensitive) == 0) {
			return true;
		}
	}
	return false;
}

bool slurp(const QString &path, QByteArray &outBytes, QString &outError)
{
	QFile file(path);
	if(!file.open(QIODevice::ReadOnly)) {
		outError = file.errorString();
		return false;
	}

	qint64 size = file.size();
	if(size < 0) {
		outError = file.errorString();
		file.close();
		return false;
	}

	if(size >= std::numeric_limits<compat::sizetype>::max()) {
		file.close();
		outError = QCoreApplication::translate(
			"utils::paths", "File size out of bounds");
		return false;
	}

	outBytes.resize(size);
	qint64 read = file.read(outBytes.data(), size);
	if(read == -1) {
		outError = file.errorString();
		file.close();
		return false;
	} else if(read != size) {
		file.close();
		outError = QCoreApplication::translate(
			"utils::paths", "Could not read entire file");
		return false;
	} else {
		file.close();
		return true;
	}
}

namespace {
static bool copyFileWith(
	const QString &sourcePath, const QString &targetPath,
	QFileDevice &targetFile, QString &outError)
{
	QFile sourceFile(sourcePath);
	if(!sourceFile.open(QIODevice::ReadOnly)) {
		outError = QCoreApplication::translate(
					   "utils::paths", "Error opening source file '%1': %2")
					   .arg(sourcePath, sourceFile.errorString());
		return false;
	}

#ifdef Q_OS_ANDROID
	QIODevice::OpenMode writeOpenFlags =
		QIODevice::WriteOnly | QIODevice::Truncate;
#else
	QIODevice::OpenMode writeOpenFlags = QIODevice::WriteOnly;
#endif
	if(!targetFile.open(writeOpenFlags)) {
		outError = QCoreApplication::translate(
					   "utils::paths", "Error opening target file '%1': %2")
					   .arg(targetPath, targetFile.errorString());
		return false;
	}

	return copyFileContents(sourceFile, targetFile, outError);
}
}

bool copyFile(
	const QString &sourcePath, const QString &targetPath, QString &outError)
{
	QFile targetFile(targetPath);
	return copyFileWith(sourcePath, targetPath, targetFile, outError);
}

bool copySaveFile(
	const QString &sourcePath, const QString &targetPath, QString &outError)
{
	QSaveFile targetFile(targetPath);
	targetFile.setDirectWriteFallback(true);

	if(!copyFileWith(sourcePath, targetPath, targetFile, outError)) {
		return false;
	}

	if(!targetFile.commit()) {
		outError = QCoreApplication::translate(
					   "utils::paths", "Failed to commit target file: %1")
					   .arg(targetFile.errorString());
		return false;
	}

	return true;
}

bool copyFileContents(
	QFileDevice &sourceFile, QFileDevice &targetFile, QString &outError)
{
	QByteArray buffer;
	buffer.resize(BUFSIZ);
	while(true) {
		qint64 read = sourceFile.read(buffer.data(), BUFSIZ);
		if(read < 0) {
			outError =
				QCoreApplication::translate(
					"utils::paths", "Error reading from source file '%1': %2")
					.arg(sourceFile.fileName(), sourceFile.errorString());
			return false;
		} else if(read > 0) {
			qint64 written = targetFile.write(buffer, read);
			if(written < 0) {
				outError =
					QCoreApplication::translate(
						"utils::paths",
						"Error writing %1 byte(s) to target file '%2': %3")
						.arg(
							QString::number(read), targetFile.fileName(),
							targetFile.errorString());
				return false;
			} else if(written != read) {
				outError =
					QCoreApplication::translate(
						"utils::paths", "Tried to write %1 byte(s) to target "
										"file '%2', but only wrote %3")
						.arg(
							QString::number(read), targetFile.fileName(),
							QString::number(written));
				return false;
			}
		} else {
			if(targetFile.flush()) {
				return true;
			} else {
				outError =
					QCoreApplication::translate(
						"utils::paths", "Error flushing target file '%1': %2")
						.arg(targetFile.fileName(), targetFile.errorString());
				return false;
			}
		}
	}
}

}
