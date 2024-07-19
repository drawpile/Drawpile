// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/filewrangler.h"
#include "desktop/main.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/document.h"
#include "libclient/import/canvasloaderrunnable.h"
#include "libshared/util/paths.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QImageReader>
#include <QLoggingCategory>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QThreadPool>
#include <dpcommon/platform_qt.h>
#if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
#	include "desktop/dialogs/filetypedialog.h"
#endif

Q_LOGGING_CATEGORY(lcDpFileWrangler, "net.drawpile.filewrangler", QtWarningMsg)


FileWrangler::FileWrangler(QWidget *parent)
	: QObject{parent}
{
}

void FileWrangler::openAvatar(const ImageOpenFn &imageOpenCompleted)
{
	openImageFileContent(tr("Add Avatar"), imageOpenCompleted);
}

void FileWrangler::openMain(const MainOpenFn &onOpen) const
{
	openMainContent(
		tr("Open"), LastPath::IMAGE,
		utils::fileFormatFilterList(utils::FileFormatOption::OpenEverything),
		onOpen);
}

QStringList FileWrangler::openAnimationFramesImport() const
{
	QStringList paths = QFileDialog::getOpenFileNames(
		parentWidget(), tr("Import Animation Frames"),
		getLastPath(LastPath::ANIMATION_FRAMES),
		tr("%1 (%2)").arg(
			tr("Frame Images"), cmake_config::file_group::flatImage()));
	if(!paths.isEmpty()) {
		updateLastPath(LastPath::ANIMATION_FRAMES, paths.first());
	}
	return paths;
}

void FileWrangler::openAnimationLayersImport(const MainOpenFn &onOpen) const
{
	openMainContent(
		tr("Import Animation from Layers"), LastPath::IMAGE,
		{QStringLiteral("%1 (%2)").arg(
			tr("Layered Images"), cmake_config::file_group::layeredImage())},
		onOpen);
}

void FileWrangler::openDebugDump(const MainOpenFn &onOpen) const
{
	openMainContent(
		tr("Open Debug Dump"), LastPath::DEBUG_DUMP,
		utils::fileFormatFilterList(utils::FileFormatOption::OpenDebugDumps),
		onOpen);
}

void FileWrangler::openBrushPack(const MainOpenFn &onOpen) const
{
	openMainContent(
		tr("Import Brushes"), LastPath::BRUSH_PACK,
		utils::fileFormatFilterList(utils::FileFormatOption::OpenBrushPack),
		onOpen);
}

void FileWrangler::openSessionBans(const BytesOpenFn &onOpen) const
{
	openBytesContent(
		tr("Import Session Bans"), LastPath::SESSION_BANS,
		utils::fileFormatFilterList(utils::FileFormatOption::SessionBans),
		onOpen);
}

void FileWrangler::openAuthList(const BytesOpenFn &onOpen) const
{
	openBytesContent(
		tr("Import Roles"), LastPath::AUTH_LIST,
		utils::fileFormatFilterList(utils::FileFormatOption::AuthList), onOpen);
}

void FileWrangler::openCanvasState(
	const CanvasStateOpenBeginFn &onBegin,
	const CanvasStateOpenSuccessFn &onSuccess,
	const CanvasStateOpenErrorFn &onError) const
{
#ifdef __EMSCRIPTEN__
	OpenFn fileOpenCompleted =
		[parent = parentWidget(), onBegin, onSuccess,
		 onError](const QString &fileName, const QByteArray &fileContent) {
			onBegin(fileName);
			QString tempFileError;
			QTemporaryFile *tempFile =
				writeToTemporaryFile(fileContent, tempFileError);
			if(tempFile) {
				loadCanvasState(
					parent, tempFile->fileName(), tempFile, onSuccess, onError);
			} else {
				onError(
					tr("Error opening temporary file for %1.").arg(fileName),
					tempFileError);
			}
		};
#else
	OpenFn fileOpenCompleted = [parent = parentWidget(), onBegin, onSuccess,
								onError](const QString &fileName) {
		onBegin(fileName);
		loadCanvasState(parent, fileName, nullptr, onSuccess, onError);
	};
#endif
	showOpenFileContentDialog(
		tr("Open Image"), LastPath::IMAGE,
		utils::fileFormatFilterList(utils::FileFormatOption::OpenImages),
		fileOpenCompleted);
}

void FileWrangler::openBrushThumbnail(const ImageOpenFn &imageOpenCompleted)
{
	openImageFileContent(tr("Set Brush Preset Thumbnail"), imageOpenCompleted);
}

void FileWrangler::openPasteImage(const ImageOpenFn &imageOpenCompleted)
{
	openImageFileContent(tr("Paste File"), imageOpenCompleted);
}


#ifndef __EMSCRIPTEN__
QStringList FileWrangler::getImportCertificatePaths(const QString &title) const
{
	QString nameFilter = getEffectiveFilter({
		tr("Certificates (%1)").arg("*.pem *.crt *.cer"),
		QApplication::tr("All files (*)"),
	});

	// TODO: QFileDialog static methods do not create sheet dialogs on macOS
	// when appropriate, nor do they allow setting the accept button label, nor
	// do they set the appropriate file mode to prevent selecting non-existing
	// files, so this code should be abstracted out to get rid of those areas
	// that use static methods where they do not do the right thing.
	QFileDialog dialog(parentWidget(), title, QString(), nameFilter);
	dialog.setLabelText(QFileDialog::Accept, tr("Import"));
	dialog.setFileMode(QFileDialog::ExistingFiles);
	dialog.setOption(QFileDialog::ReadOnly);
	dialog.setWindowModality(Qt::WindowModal);
	dialog.setSupportedSchemes({QStringLiteral("file")});
	if(dialog.exec() == QDialog::Accepted) {
		return dialog.selectedFiles();
	} else {
		return {};
	}
}
#endif

QString FileWrangler::saveImage(Document *doc) const
{
	QString path = doc->currentPath();
	DP_SaveImageType type = doc->currentType();
	qCDebug(
		lcDpFileWrangler, "saveImage path='%s', type=%d", qUtf8Printable(path),
		int(type));
	if(path.isEmpty() || type == DP_SAVE_IMAGE_UNKNOWN) {
		return saveImageAs(doc, false);
	} else if(confirmFlatten(doc, path, type)) {
		doc->saveCanvasAs(path, type, false);
		return path;
	} else {
		return QString();
	}
}

QString FileWrangler::saveImageAs(Document *doc, bool exported) const
{
	qCDebug(lcDpFileWrangler, "saveImageAs exported=%d", int(exported));
	QString selectedFilter;
	QStringList filters =
		utils::fileFormatFilterList(utils::FileFormatOption::SaveImages);
	QString extension;
	if(exported) {
		extension = QStringLiteral(".png");
		for(const QString &filter : filters) {
			if(filter.contains(QStringLiteral("*.png"))) {
				selectedFilter = filter;
				break;
			}
		}
	} else {
		extension = QStringLiteral(".ora");
	}

	QString lastPath = getCurrentPathOrUntitled(doc, extension);
	if(exported && !lastPath.isEmpty()) {
		replaceExtension(lastPath, extension);
	}

	QString intendedName;
	QString filename = showSaveFileDialogFilters(
		exported ? tr("Export Image") : tr("Save Image"), LastPath::IMAGE,
		extension, filters, &selectedFilter, lastPath, &intendedName);
	bool haveFilename = !filename.isEmpty();
	DP_SaveImageType type =
		haveFilename ? guessType(intendedName) : DP_SAVE_IMAGE_UNKNOWN;

	if(haveFilename && (exported || confirmFlatten(doc, filename, type))) {
		qCDebug(
			lcDpFileWrangler, "Saving canvas as '%s'",
			qUtf8Printable(filename));
		doc->saveCanvasAs(filename, type, exported);
		return filename;
	} else {
		qCDebug(lcDpFileWrangler, "Not saving canvas");
		return QString{};
	}
}

QString FileWrangler::savePreResetImageAs(
	Document *doc, const drawdance::CanvasState &canvasState) const
{
	QString selectedFilter;
	QString intendedName;
	QString path = showSaveFileDialog(
		tr("Save Pre-Reset Image"), LastPath::IMAGE, ".ora",
		utils::FileFormatOption::SaveImages, &selectedFilter,
		getCurrentPathOrUntitled(doc, QStringLiteral(".ora")), &intendedName);
	DP_SaveImageType type = guessType(intendedName);

	if(!path.isEmpty() && confirmFlatten(doc, path, type)) {
		doc->saveCanvasStateAs(path, type, canvasState, false, false);
		return path;
	} else {
		return QString{};
	}
}

QString FileWrangler::saveSelectionAs(Document *doc) const
{
	QString selectedFilter = "PNG (*.png)";
	QString filename = showSaveFileDialog(
		tr("Save Selection"), LastPath::IMAGE, ".png",
		utils::FileFormatOption::SaveImages |
			utils::FileFormatOption::QtImagesOnly,
		&selectedFilter);

	if(!filename.isEmpty()) {
		doc->saveSelection(filename);
		return filename;
	} else {
		return QString{};
	}
}

QString FileWrangler::getSaveRecordingPath() const
{
	// Recordings and images use the same last path setting, since they're
	// opened from the same dialog too. Just saving them works differently.
	return showSaveFileDialog(
		tr("Record Session"), LastPath::IMAGE, ".dprec",
		utils::FileFormatOption::SaveRecordings);
}

QString FileWrangler::getSaveTemplatePath() const
{
	return showSaveFileDialog(
		tr("Save Session Template"), LastPath::IMAGE, ".dprec",
		utils::FileFormatOption::SaveRecordings);
}

QString FileWrangler::getSaveAnimationFramesPath() const
{
	QString dirname = QFileDialog::getExistingDirectory(
		parentWidget(), tr("Save Animation Frames"),
		getLastPath(LastPath::ANIMATION_FRAMES));
	if(dirname.isEmpty()) {
		return QString{};
	} else {
		setLastPath(LastPath::ANIMATION_FRAMES, dirname);
		return dirname;
	}
}

QString FileWrangler::getSaveAnimationGifPath() const
{
	return showSaveFileDialog(
		tr("Export Animated GIF"), LastPath::IMAGE, ".gif",
		utils::FileFormatOption::SaveGif);
}

QString FileWrangler::getSaveAnimationZipPath() const
{
	return showSaveFileDialogFilters(
		tr("Export Frames in ZIP"), LastPath::IMAGE, ".zip",
		{QStringLiteral("ZIP (*.zip)")});
}

QString FileWrangler::getSaveAnimationMp4Path() const
{
	return showSaveFileDialogFilters(
		tr("Export MP4 Video"), LastPath::IMAGE, ".mp4",
		{QStringLiteral("MP4 (*.mp4)")});
}

QString FileWrangler::getSaveAnimationWebmPath() const
{
	return showSaveFileDialogFilters(
		tr("Export WEBM Video"), LastPath::IMAGE, ".webm",
		{QStringLiteral("WEBM (*.webm)")});
}

QString FileWrangler::getSaveAnimationWebpPath() const
{
	return showSaveFileDialogFilters(
		tr("Export Animated WEBP"), LastPath::IMAGE, ".webp",
		{QStringLiteral("WEBP (*.webp)")});
}

QString FileWrangler::getSavePerformanceProfilePath() const
{
	return showSaveFileDialog(
		tr("Performance Profile"), LastPath::PERFORMANCE_PROFILE, ".dpperf",
		utils::FileFormatOption::SaveProfile);
}

QString FileWrangler::getSaveTabletEventLogPath() const
{
	return showSaveFileDialog(
		tr("Tablet Event Log"), LastPath::TABLET_EVENT_LOG, ".dplog",
		utils::FileFormatOption::SaveEventLog);
}

#ifdef HAVE_VIDEO_EXPORT
QString FileWrangler::getSaveFfmpegMp4Path() const
{
	return showSaveFileDialog(
		tr("Export MP4 Video"), LastPath::IMAGE, ".mp4",
		utils::FileFormatOption::SaveMp4);
}

QString FileWrangler::getSaveFfmpegWebmPath() const
{
	return showSaveFileDialog(
		tr("Export WebM Video"), LastPath::IMAGE, ".webm",
		utils::FileFormatOption::SaveWebm);
}

QString FileWrangler::getSaveFfmpegCustomPath() const
{
	return showSaveFileDialog(
		tr("Export Custom FFmpeg Video"), LastPath::IMAGE, ".mp4",
		utils::FileFormatOption::SaveAllFiles);
}

QString FileWrangler::getSaveImageSeriesPath() const
{
	QString dirname = QFileDialog::getExistingDirectory(
		parentWidget(), tr("Save Image Series"),
		getLastPath(LastPath::ANIMATION_FRAMES));
	if(dirname.isEmpty()) {
		return QString{};
	} else {
		setLastPath(LastPath::ANIMATION_FRAMES, dirname);
		return dirname;
	}
}
#endif

#ifdef __EMSCRIPTEN__
void FileWrangler::downloadImage(Document *doc)
{
	dialogs::FileTypeDialog nameAndTypeDialog(
		doc->downloadName(),
		utils::fileFormatFilterList(utils::FileFormatOption::SaveImages),
		parentWidget());
	if(nameAndTypeDialog.exec() != QDialog::Accepted) {
		return;
	}

	QString filename = nameAndTypeDialog.name();
	if(filename.isEmpty()) {
		return;
	}

	doc->setDownloadName(filename);
	QString filter = nameAndTypeDialog.type();
	QString selectedExt = guessExtension(filter, QStringLiteral(".ora"));
	replaceExtension(filename, selectedExt);

	DP_SaveImageType type = guessType(filename);
	QTemporaryDir *tempDir = new QTemporaryDir;
	doc->downloadCanvas(filename, type, tempDir);
}

bool FileWrangler::downloadPreResetImage(
	Document *doc, const drawdance::CanvasState &canvasState) const
{
	dialogs::FileTypeDialog nameAndTypeDialog(
		doc->downloadName(),
		utils::fileFormatFilterList(utils::FileFormatOption::SaveImages),
		parentWidget());
	if(nameAndTypeDialog.exec() != QDialog::Accepted) {
		return false;
	}

	QString filename = nameAndTypeDialog.name();
	if(filename.isEmpty()) {
		return false;
	}

	doc->setDownloadName(filename);
	QString filter = nameAndTypeDialog.type();
	QString selectedExt = guessExtension(filter, QStringLiteral(".ora"));
	replaceExtension(filename, selectedExt);

	DP_SaveImageType type = guessType(filename);
	QTemporaryDir *tempDir = new QTemporaryDir;
	doc->downloadCanvasState(filename, type, tempDir, canvasState);
	return true;
}

void FileWrangler::downloadSelection(Document *doc)
{
	dialogs::FileTypeDialog nameAndTypeDialog(
		doc->downloadName(),
		utils::fileFormatFilterList(
			utils::FileFormatOption::SaveImages |
			utils::FileFormatOption::QtImagesOnly),
		parentWidget());
	if(nameAndTypeDialog.exec() != QDialog::Accepted) {
		return;
	}

	QString filename = nameAndTypeDialog.name();
	if(filename.isEmpty()) {
		return;
	}

	doc->setDownloadName(filename);
	QString filter = nameAndTypeDialog.type();
	QString selectedExt = guessExtension(filter, QStringLiteral(".png"));
	replaceExtension(filename, selectedExt);
	doc->downloadSelection(filename);
}

void FileWrangler::saveFileContent(
	const QString &defaultName, const QByteArray &bytes) const
{
	QFileDialog::saveFileContent(bytes, defaultName);
}
#endif


void FileWrangler::saveBrushPack(const PathSaveFn &onSave) const
{
	savePathContent(
		tr("Export Brushes"), LastPath::BRUSH_PACK, QStringLiteral(".zip"),
		QStringLiteral("dpbrushes.zip"),
		utils::fileFormatFilterList(utils::FileFormatOption::SaveBrushPack),
		onSave);
}

bool FileWrangler::saveSessionBans(
	const QByteArray &bytes, QString *outError) const
{
	return saveBytesContent(
		tr("Export Session Bans"), LastPath::SESSION_BANS,
		QStringLiteral(".dpbans"), QStringLiteral("bans.dpbans"),
		utils::fileFormatFilterList(utils::FileFormatOption::SaveSessionBans),
		bytes, outError);
}

bool FileWrangler::saveAuthList(
	const QByteArray &bytes, QString *outError) const
{
	return saveBytesContent(
		tr("Export Roles"), LastPath::AUTH_LIST, QStringLiteral(".dproles"),
		QStringLiteral("roles.dproles"),
		utils::fileFormatFilterList(utils::FileFormatOption::SaveAuthList),
		bytes, outError);
}

bool FileWrangler::saveLogFile(
	const QString &defaultName, const QByteArray &bytes,
	QString *outError) const
{
	return saveBytesContent(
		tr("Log File"), LastPath::LOG_FILE, QStringLiteral(".txt"), defaultName,
		utils::fileFormatFilterList(utils::FileFormatOption::SaveText), bytes,
		outError);
}


bool FileWrangler::confirmFlatten(
	Document *doc, QString &path, DP_SaveImageType &type) const
{
#ifdef Q_OS_ANDROID
	// We're not allowed to change the file extension on Android, all we could
	// do at this point is continue or cancel. So we'll just keep going.
	Q_UNUSED(doc);
	Q_UNUSED(path);
	Q_UNUSED(type);
	return true;
#else
	// If the image can be flattened without losing any information, we don't
	// need to confirm anything.
	bool needsOra = type != DP_SAVE_IMAGE_ORA &&
					doc->canvas()->paintEngine()->needsOpenRaster();
	if(!needsOra) {
		return true;
	}

	QMessageBox box(
		QMessageBox::Information, tr("Save Image"),
		type == DP_SAVE_IMAGE_PSD
			? tr("The PSD format lacks support for annotations, the animation "
				 "timeline and some blend modes. If you want those to be "
				 "retained properly, you must save an ORA file.")
			: tr("The selected format will save a merged image. If you want to "
				 "retain layers, annotations and the animation timeline, you "
				 "must save an ORA file."),
		QMessageBox::Cancel, parentWidget());

	QString format;
	switch(type) {
	case DP_SAVE_IMAGE_UNKNOWN:
	case DP_SAVE_IMAGE_ORA:
		break;
	case DP_SAVE_IMAGE_PSD:
		format = QStringLiteral("PSD");
		break;
	case DP_SAVE_IMAGE_PNG:
		format = QStringLiteral("PNG");
		break;
	case DP_SAVE_IMAGE_JPEG:
		format = QStringLiteral("JPEG");
		break;
	}
	box.addButton(
		format.isEmpty() ? tr("Save as Selected Format")
						 : tr("Save as %1").arg(format),
		QMessageBox::AcceptRole);

	QPushButton *saveOraButton =
		box.addButton(tr("Save as ORA"), QMessageBox::ActionRole);

	if(box.exec() == QMessageBox::Cancel) {
		// Cancel saving altogether.
		return false;
	} else if(box.clickedButton() == saveOraButton) {
		// Change to saving as an ORA file.
		replaceExtension(path, ".ora");
		setLastPath(LastPath::IMAGE, path);
		type = DP_SAVE_IMAGE_ORA;
		return true;
	} else {
		// Save the file as-is.
		return true;
	}
#endif
}

QString FileWrangler::guessExtension(
	const QString &selectedFilter, const QString &fallbackExt)
{
	QRegularExpressionMatch match =
		QRegularExpression{"\\*(\\.\\w+)"}.match(selectedFilter);
	if(match.hasMatch()) {
		return match.captured(1);
	} else {
		return fallbackExt;
	}
}

void FileWrangler::replaceExtension(QString &filename, const QString &ext)
{
	qCDebug(
		lcDpFileWrangler, "replaceExtension of '%s' with '%s'",
		qUtf8Printable(filename), qUtf8Printable(ext));
	QRegularExpressionMatch match =
		QRegularExpression{"\\.\\w*\\z"}.match(filename);
	if(match.hasMatch()) {
		filename = filename.left(match.capturedStart()) + ext;
		qCDebug(
			lcDpFileWrangler, "Match; replaced to result in '%s'",
			qUtf8Printable(filename));
	} else {
		filename += ext;
		qCDebug(
			lcDpFileWrangler, "No match; appended to result in '%s'",
			qUtf8Printable(filename));
	}
}

DP_SaveImageType FileWrangler::guessType(const QString &intendedName)
{
	DP_SaveImageType type =
		DP_save_image_type_guess(qUtf8Printable(intendedName));
	qCDebug(
		lcDpFileWrangler, "Guessed type %d for '%s'", int(type),
		qUtf8Printable(intendedName));
	return type;
}

QString FileWrangler::getCurrentPathOrUntitled(
	Document *doc, const QString &defaultExtension)
{
	qCDebug(
		lcDpFileWrangler, "getCurrentPathOrUntitled defaultExtension='%s'",
		qUtf8Printable(defaultExtension));
	QString path = doc->currentPath();
	if(path.isEmpty()) {
		qCDebug(lcDpFileWrangler, "Current path is empty");
#ifdef Q_OS_ANDROID
		return tr("Untitled") + defaultExtension;
#else
		path = getLastPath(LastPath::IMAGE);
		if(!path.isEmpty()) {
			return QFileInfo(path).absoluteDir().filePath(
				tr("Untitled") + defaultExtension);
		}
#endif
	}
	qCDebug(
		lcDpFileWrangler, "Using document current path '%s'",
		qUtf8Printable(path));
	return path;
}

bool FileWrangler::needsOra(Document *doc)
{
	return doc->canvas()->paintEngine()->needsOpenRaster();
}

QString FileWrangler::getLastPath(LastPath type, const QString &ext)
{
	const auto paths = dpApp().settings().lastFileOpenPaths();
	QString key = getLastPathKey(type);
	QString filename = paths.value(key).toString();
	qCDebug(
		lcDpFileWrangler, "getLastPath type=%d ext='%s' key='%s': '%s'",
		int(type), qUtf8Printable(ext), qUtf8Printable(key),
		qUtf8Printable(filename));
	if(filename.isEmpty()) {
		return getDefaultLastPath(type, ext);
	} else {
		if(!ext.isEmpty()) {
			replaceExtension(filename, ext);
		}
		return filename;
	}
}

void FileWrangler::setLastPath(LastPath type, const QString &path)
{
	QString key = getLastPathKey(type);
	qCDebug(
		lcDpFileWrangler, "setLastPath type=%d path='%s' key='%s'", int(type),
		qUtf8Printable(path), qUtf8Printable(key));
	auto &settings = dpApp().settings();
	auto paths = settings.lastFileOpenPaths();
	paths[key] = path;
	settings.setLastFileOpenPaths(paths);
}

void FileWrangler::updateLastPath(LastPath type, const QString &path)
{
#ifdef Q_OS_ANDROID
	// On Android, the last path is really just the name of the last
	// file, since paths are weird content URIs that don't interact
	// with the native Android file picker in any kind of sensible
	// way.
	QString basename = utils::paths::extractBasename(path);
	if(!basename.isEmpty()) {
		setLastPath(type, basename);
	}
#else
	setLastPath(type, path);
#endif
}

QString FileWrangler::getLastPathKey(LastPath type)
{
	switch(type) {
	case LastPath::IMAGE:
		return QStringLiteral("image");
	case LastPath::ANIMATION_FRAMES:
		return QStringLiteral("animationframes");
	case LastPath::PERFORMANCE_PROFILE:
		return QStringLiteral("performanceprofile");
	case LastPath::TABLET_EVENT_LOG:
		return QStringLiteral("tableteventlog");
	case LastPath::DEBUG_DUMP:
		return QStringLiteral("debugdump");
	case LastPath::BRUSH_PACK:
		return QStringLiteral("brushpack");
	case LastPath::SESSION_BANS:
		return QStringLiteral("sessionbans");
	case LastPath::AUTH_LIST:
		return QStringLiteral("authlist");
	case LastPath::LOG_FILE:
		return QStringLiteral("logfile");
	}
	return QStringLiteral("unknown");
}

QString FileWrangler::getDefaultLastPath(LastPath type, const QString &ext)
{
	QString path;
	switch(type) {
	case LastPath::ANIMATION_FRAMES:
		path =
			QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
		break;
	case LastPath::DEBUG_DUMP:
		path = utils::paths::writablePath("dumps");
		break;
	default:
		//: %1 will be a file extension, like .ora or .png or something.
		path = QDir(QStandardPaths::writableLocation(
						QStandardPaths::PicturesLocation))
				   .absoluteFilePath(tr("Untitled%1").arg(ext));
		break;
	}
	qCDebug(
		lcDpFileWrangler, "getDefaultLastPath type=%d ext='%s': '%s'",
		int(type), qUtf8Printable(ext), qUtf8Printable(path));
	return path;
}

void FileWrangler::openImageFileContent(
	const QString &title, const ImageOpenFn &imageOpenCompleted) const
{
#ifdef __EMSCRIPTEN__
	OpenFn fileOpenCompleted =
		[imageOpenCompleted](const QString &, const QByteArray &fileContent) {
			QImage img;
			img.loadFromData(fileContent);
			imageOpenCompleted(img);
		};
#else
	OpenFn fileOpenCompleted = [imageOpenCompleted](const QString &fileName) {
		QImage img;
		img.load(fileName);
		imageOpenCompleted(img);
	};
#endif
	showOpenFileContentDialog(
		title, LastPath::IMAGE, utils::fileFormatFilterList(utils::OpenImages),
		fileOpenCompleted);
}

void FileWrangler::openMainContent(
	const QString &title, LastPath type, const QStringList &filters,
	const MainOpenFn &onOpen) const
{
#ifdef __EMSCRIPTEN__
	OpenFn fileOpenCompleted =
		[parent = parentWidget(),
		 onOpen](const QString &fileName, const QByteArray &fileContent) {
			QString tempFileError;
			QTemporaryFile *tempFile =
				writeToTemporaryFile(fileContent, tempFileError);
			if(tempFile) {
				onOpen(tempFile->fileName(), tempFile);
			} else {
				qWarning(
					"Error opening temporary file for %s: %s",
					qUtf8Printable(fileName), qUtf8Printable(tempFileError));
			}
		};
#else
	OpenFn fileOpenCompleted = [parent = parentWidget(),
								onOpen](const QString &fileName) {
		onOpen(fileName, nullptr);
	};
#endif
	showOpenFileContentDialog(title, type, filters, fileOpenCompleted);
}

void FileWrangler::openBytesContent(
	const QString &title, LastPath type, const QStringList &filters,
	const BytesOpenFn &onOpen) const
{
#ifdef __EMSCRIPTEN__
	OpenFn fileOpenCompleted =
		[parent = parentWidget(),
		 onOpen](const QString &, const QByteArray &fileContent) {
			onOpen(nullptr, &fileContent);
		};
#else
	OpenFn fileOpenCompleted = [parent = parentWidget(),
								onOpen](const QString &fileName) {
		onOpen(&fileName, nullptr);
	};
#endif
	showOpenFileContentDialog(title, type, filters, fileOpenCompleted);
}

void FileWrangler::loadCanvasState(
	QWidget *parent, const QString &path, QTemporaryFile *temporaryFileOrNull,
	const CanvasStateOpenSuccessFn &onSuccess,
	const CanvasStateOpenErrorFn &onError)
{
	CanvasLoaderRunnable *loader = new CanvasLoaderRunnable(path);
	loader->setAutoDelete(false);
	parent->connect(
		loader, &CanvasLoaderRunnable::loadComplete, parent,
		[=](const QString &error, const QString &detail) {
			delete temporaryFileOrNull;
			const drawdance::CanvasState &canvasState = loader->canvasState();
			if(canvasState.isNull()) {
				onError(error, detail);
			} else {
				onSuccess(canvasState);
			}
			loader->deleteLater();
		},
		Qt::QueuedConnection);
	QThreadPool::globalInstance()->start(loader);
}

void FileWrangler::savePathContent(
	const QString &title, LastPath type, const QString &ext,
	const QString &defaultName, const QStringList &filters,
	const PathSaveFn &onSave) const
{
#ifdef __EMSCRIPTEN__
	Q_UNUSED(type);
	Q_UNUSED(ext);
	Q_UNUSED(filters);
	QTemporaryDir tempDir;
	if(tempDir.isValid()) {
		QString path = tempDir.filePath(defaultName);
		if(onSave(path)) {
			QFile file(path);
			if(file.open(QIODevice::ReadOnly)) {
				QFileDialog::saveFileContent(file.readAll(), defaultName);
			} else {
				qWarning(
					"%s: Error reading temporary file: %s",
					qUtf8Printable(title), qUtf8Printable(file.errorString()));
			}
		}
	} else {
		qWarning(
			"%s: Error creating temporary directory: %s", qUtf8Printable(title),
			qUtf8Printable(tempDir.errorString()));
	}
#else
	Q_UNUSED(defaultName);
	QString path = showSaveFileDialogFilters(title, type, ext, filters);
	if(!path.isEmpty()) {
		onSave(path);
	}
#endif
}

bool FileWrangler::saveBytesContent(
	const QString &title, LastPath type, const QString &ext,
	const QString &defaultName, const QStringList &filters,
	const QByteArray &bytes, QString *outError) const
{
#ifdef __EMSCRIPTEN__
	Q_UNUSED(title);
	Q_UNUSED(type);
	Q_UNUSED(ext);
	Q_UNUSED(filters);
	Q_UNUSED(outError);
	QFileDialog::saveFileContent(bytes, defaultName);
	return true;
#else
	Q_UNUSED(defaultName);
	QString path = showSaveFileDialogFilters(title, type, ext, filters);
	if(path.isEmpty()) {
		return true;
	} else {
		QFile file(path);
		bool ok = file.open(DP_QT_WRITE_FLAGS) &&
				  file.write(bytes) == bytes.size() && file.flush();
		if(!ok && outError) {
			*outError = file.errorString();
		}
		return ok;
	}
#endif
}

QString FileWrangler::showOpenFileDialog(
	const QString &title, LastPath type, utils::FileFormatOptions formats) const
{
	return showOpenFileDialogFilters(
		title, type, utils::fileFormatFilterList(formats));
}

QString FileWrangler::showOpenFileDialogFilters(
	const QString &title, LastPath type, const QStringList &filters) const
{
	QString filename = QFileDialog::getOpenFileName(
		parentWidget(), title, getLastPath(type), getEffectiveFilter(filters));
	if(filename.isEmpty()) {
		return QString{};
	} else {
#ifdef Q_OS_ANDROID
		// On Android, the last path is really just the name of the last file,
		// since paths are weird content URIs that don't interact with the
		// native Android file picker in any kind of sensible way.
		QString basename = utils::paths::extractBasename(filename);
		if(!basename.isEmpty()) {
			setLastPath(type, basename);
		}
#else
		setLastPath(type, filename);
#endif
		return filename;
	}
}

QString FileWrangler::showSaveFileDialog(
	const QString &title, LastPath type, const QString &ext,
	utils::FileFormatOptions formats, QString *selectedFilter,
	std::optional<QString> lastPath, QString *outIntendedName) const
{
	return showSaveFileDialogFilters(
		title, type, ext, utils::fileFormatFilterList(formats), selectedFilter,
		lastPath, outIntendedName);
}

QString FileWrangler::showSaveFileDialogFilters(
	const QString &title, LastPath type, const QString &ext,
	const QStringList &filters, QString *selectedFilter,
	std::optional<QString> lastPath, QString *outIntendedName) const
{
	qCDebug(
		lcDpFileWrangler, "showSaveFileDialog type=%d ext='%s' lastPath='%s'",
		int(type), qUtf8Printable(ext),
		lastPath.has_value() ? qUtf8Printable(lastPath.value()) : "<empty>");

	QString filename;
	QString selectedNameFilter;
#ifdef Q_OS_ANDROID
	Q_UNUSED(title);
	Q_UNUSED(ext);

	QFileDialog fileDialog(parentWidget());
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);

	// File handling on Android is all kinds of terrible. We can only use
	// the path that the user gave us, but have no way to restrict them to
	// sensible file extensions. If they don't provide a proper extension,
	// we aren't allowed to change it. To make matters worse, it will
	// automatically create a file of zero size that we can't delete. So we
	// ask the user for a name and file type first, using that as the
	// initial name and hope that most users will intuitively just pick a
	// directory to save the file in, rather than decide to break the path
	// after the fact.
	dialogs::FileTypeDialog nameAndTypeDialog{
		lastPath.has_value() ? lastPath.value() : getLastPath(type), filters,
		parentWidget()};
	if(nameAndTypeDialog.exec() != QDialog::Accepted) {
		return QString{};
	}

	QString initialFilename = nameAndTypeDialog.name();
	if(initialFilename.isEmpty()) {
		return QString{};
	}

	QString filter = nameAndTypeDialog.type();
	fileDialog.setNameFilter(filter);
	if(selectedFilter) {
		*selectedFilter = filter;
	}

	QString selectedExt = guessExtension(filter, ext);
	replaceExtension(initialFilename, selectedExt);

	// Hack taken from Krita: on Android, the dialog title will be used as
	// the initial filename for some reason. So we set that to our basename.
	fileDialog.setWindowTitle(initialFilename);
	// This doesn't seem to do anything useful, but let's set it anyway in
	// case a later Qt version fixes the window title thing.
	fileDialog.selectFile(initialFilename);

	if(fileDialog.exec() == QDialog::Accepted &&
	   !fileDialog.selectedFiles().isEmpty()) {
		filename = fileDialog.selectedFiles().first().trimmed();
		selectedNameFilter = fileDialog.selectedNameFilter();
	}
#else
	if(selectedFilter) {
		selectedNameFilter = *selectedFilter;
	}
	filename =
		QFileDialog::getSaveFileName(
			parentWidget(), title,
			lastPath.has_value() ? lastPath.value() : getLastPath(type, ext),
			filters.join(QStringLiteral(";;")), &selectedNameFilter)
			.trimmed();
#endif

	if(filename.isEmpty()) {
		return QString{};
	}

	// We're neither allowed to edit the chosen file name on Android, nor does
	// the URI necessarily contain a sensible extension to begin with. So we
	// just live with the user providing a bogus or missing extension.
#ifndef Q_OS_ANDROID
	bool emptySuffix = QFileInfo{filename}.completeSuffix().isEmpty();
	if(emptySuffix) {
		filename += guessExtension(selectedNameFilter, ext);
	}
#endif

#ifdef Q_OS_ANDROID
	setLastPath(type, initialFilename);
	if(outIntendedName) {
		*outIntendedName = initialFilename;
	}
#else
	setLastPath(type, filename);
	if(selectedFilter) {
		*selectedFilter = selectedNameFilter;
	}
	if(outIntendedName) {
		*outIntendedName = filename;
	}
#endif

	return filename;
}

void FileWrangler::showOpenFileContentDialog(
	const QString &title, LastPath type, const QStringList &filters,
	const OpenFn &fileOpenCompleted) const
{
	QString nameFilter = getEffectiveFilter(filters);
#ifdef __EMSCRIPTEN__
	Q_UNUSED(title);
	Q_UNUSED(type);
	QFileDialog::getOpenFileContent(
		nameFilter,
		[fileOpenCompleted](
			const QString &fileName, const QByteArray &fileContent) {
			if(!fileName.isEmpty() || !fileContent.isEmpty()) {
				fileOpenCompleted(fileName, fileContent);
			}
		});
#else
	QFileDialog *dialog =
		new QFileDialog(parentWidget(), title, getLastPath(type), nameFilter);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setWindowModality(Qt::WindowModal);
	dialog->setFileMode(QFileDialog::ExistingFile);

	std::function<void(const QString &)> fileSelected =
		[=](const QString &fileName) {
			if(!fileName.isNull()) {
				updateLastPath(type, fileName);
				fileOpenCompleted(fileName);
			}
		};
	connect(dialog, &QFileDialog::fileSelected, fileSelected);

	dialog->show();
#endif
}

QString FileWrangler::getEffectiveFilter(const QStringList &filters)
{
#ifdef Q_OS_ANDROID
	// Name filters don't work properly on Android. They are supposed to end up
	// being mapped to MIME types internally, but that doesn't seem to actually
	// work at all, causing random types to just inexplicably get grayed out or
	// hidden altogether. So we just don't pass any filters.
	Q_UNUSED(filters);
	return QString();
#else
	return filters.join(QStringLiteral(";;"));
#endif
}

QWidget *FileWrangler::parentWidget() const
{
	return qobject_cast<QWidget *>(parent());
}

QTemporaryFile *FileWrangler::writeToTemporaryFile(
	const QByteArray &fileContent, QString &outError)
{
	QTemporaryFile *tempFile = new QTemporaryFile;
	bool tempFileOk = tempFile->open() &&
					  tempFile->write(fileContent) == fileContent.size() &&
					  tempFile->flush();
	if(tempFileOk) {
		tempFile->close();
		return tempFile;
	} else {
		outError = tempFile->errorString();
		delete tempFile;
		return nullptr;
	}
}
