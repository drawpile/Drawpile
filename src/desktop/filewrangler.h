// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_FILEWRANGLER_H
#define DESKTOP_FILEWRANGLER_H
extern "C" {
#include <dpimpex/save.h>
}
#include "libclient/utils/images.h"
#include <QObject>
#include <QString>
#include <QStringList>
#include <optional>

class Document;
class QImage;
class QTemporaryFile;
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
#ifdef __EMSCRIPTEN__
	using OpenFn = std::function<void(const QString &, const QByteArray &)>;
#else
	using OpenFn = std::function<void(const QString &)>;
#endif

	using ImageOpenFn = std::function<void(QImage &, const QString &)>;

	// Gives just a path or, on Emscripten, a symbolic path and a temporary
	// file. The callee is responsible for calling delete on the latter.
	using MainOpenFn = std::function<void(const QString &, QTemporaryFile *)>;

	// Gives either a string for a file path or, on Emscripten, the bytes of the
	// file. Check if the bytes are present first, fall back to using the path.
	using BytesOpenFn =
		std::function<void(const QString *, const QByteArray *)>;

	using CanvasStateOpenBeginFn = std::function<void(const QString &)>;
	using CanvasStateOpenSuccessFn =
		std::function<void(const drawdance::CanvasState &)>;
	using CanvasStateOpenErrorFn =
		std::function<void(const QString &, const QString &)>;

	using PathSaveFn = std::function<bool(const QString &)>;

	enum class LastPath {
		IMAGE,
		ANIMATION_FRAMES,
		PERFORMANCE_PROFILE,
		TABLET_EVENT_LOG,
		DEBUG_DUMP,
		BRUSH_PACK,
		SESSION_BANS,
		AUTH_LIST,
		LOG_FILE,
	};

	FileWrangler(QWidget *parent);

	FileWrangler(const FileWrangler &) = delete;
	FileWrangler(FileWrangler &&) = delete;
	FileWrangler &operator=(const FileWrangler &) = delete;
	FileWrangler &operator=(FileWrangler &&) = delete;

	void openAvatar(const ImageOpenFn &imageOpenCompleted);
	void openBrushThumbnail(const ImageOpenFn &imageOpenCompleted);
	void openPasteImage(const ImageOpenFn &imageOpenCompleted);

	void openMain(const MainOpenFn &onOpen) const;
	QStringList openAnimationFramesImport() const;
	void openAnimationLayersImport(const MainOpenFn &onOpen) const;
	void openDebugDump(const MainOpenFn &onOpen) const;
	void openBrushPack(const MainOpenFn &onOpen) const;

	void openSessionBans(const BytesOpenFn &onOpen) const;
	void openAuthList(const BytesOpenFn &onOpen) const;

	void openCanvasState(
		const CanvasStateOpenBeginFn &onBegin,
		const CanvasStateOpenSuccessFn &onSuccess,
		const CanvasStateOpenErrorFn &onError) const;

#ifndef __EMSCRIPTEN_
	// The browser handles our certificates in Emscripten.
	QStringList getImportCertificatePaths(const QString &title) const;
#endif

	QString saveImage(Document *doc) const;
	QString saveImageAs(Document *doc, bool exported) const;
	QString savePreResetImageAs(
		Document *doc, const drawdance::CanvasState &canvasState) const;
	QString saveSelectionAs(Document *doc) const;
	QString getSaveRecordingPath() const;
	QString getSaveTemplatePath() const;
	QString getSaveAnimationGifPath() const;
	QString getSaveAnimationFramesPath() const;
	QString getSaveAnimationZipPath() const;
	QString getSaveAnimationMp4Path() const;
	QString getSaveAnimationWebmPath() const;
	QString getSaveAnimationWebpPath() const;
	QString getSavePerformanceProfilePath() const;
	QString getSaveTabletEventLogPath() const;
#ifdef HAVE_VIDEO_EXPORT
	QString getSaveFfmpegMp4Path() const;
	QString getSaveFfmpegWebmPath() const;
	QString getSaveFfmpegCustomPath() const;
	QString getSaveImageSeriesPath() const;
#endif

#ifdef __EMSCRIPTEN__
	void downloadImage(Document *doc);
	bool downloadPreResetImage(
		Document *doc, const drawdance::CanvasState &canvasState) const;
	void downloadSelection(Document *doc);
	void
	saveFileContent(const QString &defaultName, const QByteArray &bytes) const;
#endif

	void saveBrushPack(const PathSaveFn &onSave) const;
	bool saveSessionBans(const QByteArray &bytes, QString *outError) const;
	bool saveAuthList(const QByteArray &bytes, QString *outError) const;
	bool saveLogFile(
		const QString &defaultName, const QByteArray &bytes,
		QString *outError) const;

private:
	bool
	confirmFlatten(Document *doc, QString &path, DP_SaveImageType &type) const;

	static QString
	guessExtension(const QString &selectedFilter, const QString &fallbackExt);

	static void replaceExtension(QString &filename, const QString &ext);

	static DP_SaveImageType guessType(const QString &intendedName);

	static QString
	getCurrentPathOrUntitled(Document *doc, const QString &defaultExtension);

	static bool needsOra(Document *doc);

	static QString getLastPath(LastPath type, const QString &ext = QString{});
	static void setLastPath(LastPath type, const QString &path);
	static void updateLastPath(LastPath type, const QString &path);
	static QString getLastPathKey(LastPath type);
	static QString getDefaultLastPath(LastPath type, const QString &ext);

	void openImageFileContent(
		const QString &title, const ImageOpenFn &imageOpenCompleted) const;

	void openMainContent(
		const QString &title, LastPath type, const QStringList &filters,
		const MainOpenFn &onOpen) const;

	void openBytesContent(
		const QString &title, LastPath type, const QStringList &filters,
		const BytesOpenFn &onOpen) const;

	static void loadCanvasState(
		QWidget *parent, const QString &path,
		QTemporaryFile *temporaryFileOrNull,
		const CanvasStateOpenSuccessFn &onSuccess,
		const CanvasStateOpenErrorFn &onError);

	void savePathContent(
		const QString &title, LastPath type, const QString &ext,
		const QString &defaultName, const QStringList &filters,
		const PathSaveFn &onSave) const;

	bool saveBytesContent(
		const QString &title, LastPath type, const QString &ext,
		const QString &defaultName, const QStringList &filters,
		const QByteArray &bytes, QString *outError) const;

	QString showOpenFileDialog(
		const QString &title, LastPath type,
		utils::FileFormatOptions formats) const;

	QString showOpenFileDialogFilters(
		const QString &title, LastPath type, const QStringList &filters) const;

	QString showSaveFileDialog(
		const QString &title, LastPath type, const QString &ext,
		utils::FileFormatOptions formats, QString *selectedFilter = nullptr,
		std::optional<QString> lastPath = {},
		QString *outIntendedName = nullptr) const;

	QString showSaveFileDialogFilters(
		const QString &title, LastPath type, const QString &ext,
		const QStringList &filters, QString *selectedFilter = nullptr,
		std::optional<QString> lastPath = {},
		QString *outIntendedName = nullptr) const;

	void showOpenFileContentDialog(
		const QString &title, LastPath type, const QStringList &filters,
		const OpenFn &fileOpenCompleted) const;

	static QString getEffectiveFilter(const QStringList &filters);

	QWidget *parentWidget() const;

	static QTemporaryFile *
	writeToTemporaryFile(const QByteArray &fileContent, QString &outError);
};

#endif
