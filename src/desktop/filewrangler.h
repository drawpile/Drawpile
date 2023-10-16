// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILEWRANGLER_H
#define FILEWRANGLER_H

#include "libclient/utils/images.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <optional>

class Document;
class QWidget;

namespace drawdance {
class CanvasState;
}

namespace drawdance {
class CanvasState;
}

class FileWrangler final : public QObject {
	Q_OBJECT
public:
	enum class LastPath {
		IMAGE,
		ANIMATION_FRAMES,
		PERFORMANCE_PROFILE,
		TABLET_EVENT_LOG,
		DEBUG_DUMP,
		BRUSH_PACK,
		SESSION_BANS,
#ifdef Q_OS_ANDROID
		LOG_FILE,
#endif
	};

	FileWrangler(QWidget *parent);

	FileWrangler(const FileWrangler &) = delete;
	FileWrangler(FileWrangler &&) = delete;
	FileWrangler &operator=(const FileWrangler &) = delete;
	FileWrangler &operator=(FileWrangler &&) = delete;

	QStringList getImportCertificatePaths(const QString &title) const;
	QString getOpenPath() const;
	QString getOpenPasteImagePath() const;
	QString getOpenDebugDumpsPath() const;
	QString getOpenBrushPackPath() const;
	QString getOpenSessionBansPath() const;

	QString saveImage(Document *doc) const;
	QString saveImageAs(Document *doc) const;
	QString savePreResetImageAs(
		Document *doc, const drawdance::CanvasState &canvasState) const;
	QString saveSelectionAs(Document *doc) const;
	QString getSaveRecordingPath() const;
	QString getSaveTemplatePath() const;
	QString getSaveGifPath() const;
	QString getSavePerformanceProfilePath() const;
	QString getSaveTabletEventLogPath() const;
#ifdef Q_OS_ANDROID
	QString getSaveLogFilePath() const;
#else
	QString getSaveFfmpegMp4Path() const;
	QString getSaveFfmpegWebmPath() const;
	QString getSaveFfmpegCustomPath() const;
	QString getSaveAnimationFramesPath() const;
	QString getSaveImageSeriesPath() const;
#endif
	QString getSaveBrushPackPath() const;
	QString getSaveSessionBansPath() const;

private:
	bool confirmFlatten(Document *doc, QString &filename) const;

	static QString
	guessExtension(const QString &selectedFilter, const QString &fallbackExt);

	static void replaceExtension(QString &filename, const QString &ext);

	static bool needsOra(Document *doc);

	static QString getLastPath(LastPath type, const QString &ext = QString{});
	static void setLastPath(LastPath type, const QString &path);
	static QString getLastPathKey(LastPath type);
	static QString getDefaultLastPath(LastPath type, const QString &ext);

	QString showOpenFileDialog(
		const QString &title, LastPath type,
		utils::FileFormatOptions formats) const;

	QString showSaveFileDialog(
		const QString &title, LastPath type, const QString &ext,
		utils::FileFormatOptions formats, QString *selectedFilter = nullptr,
		std::optional<QString> lastPath = {}) const;

	QWidget *parentWidget() const;
};

#endif
