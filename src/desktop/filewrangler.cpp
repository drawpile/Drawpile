// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/filewrangler.h"
#include "desktop/main.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/document.h"
#include "libshared/util/paths.h"
#include <QCoreApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#ifdef Q_OS_ANDROID
#	include "desktop/dialogs/androidfiledialog.h"
#endif


FileWrangler::FileWrangler(QWidget *parent)
	: QObject{parent}
{
}

QStringList FileWrangler::getImportCertificatePaths(const QString &title) const
{
	const auto filter = tr("Certificates (%1)").arg("*.pem *.crt *.cer") +
						";;" + QApplication::tr("All files (*)");

	// TODO: QFileDialog static methods do not create sheet dialogs on macOS
	// when appropriate, nor do they allow setting the accept button label, nor
	// do they set the appropriate file mode to prevent selecting non-existing
	// files, so this code should be abstracted out to get rid of those areas
	// that use static methods where they do not do the right thing.
	QFileDialog dialog(parentWidget(), title, QString(), filter);
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

QString FileWrangler::getOpenPath() const
{
	return showOpenFileDialog(
		tr("Open"), LastPath::IMAGE, utils::FileFormatOption::OpenEverything);
}

QString FileWrangler::getOpenPasteImagePath() const
{
	return showOpenFileDialog(
		tr("Paste Image"), LastPath::IMAGE,
		utils::FileFormatOption::OpenImages |
			utils::FileFormatOption::QtImagesOnly);
}

QString FileWrangler::getOpenDebugDumpsPath() const
{
	return showOpenFileDialog(
		tr("Open Debug Dump"), LastPath::DEBUG_DUMP,
		utils::FileFormatOption::OpenBrushPack);
}

QString FileWrangler::getOpenBrushPackPath() const
{
	return showOpenFileDialog(
		tr("Import Brush Pack"), LastPath::BRUSH_PACK,
		utils::FileFormatOption::BrushPack);
}

QString FileWrangler::getOpenSessionBansPath() const
{
	return showOpenFileDialog(
		tr("Import Session Bans"), LastPath::SESSION_BANS,
		utils::FileFormatOption::SessionBans);
}


QString FileWrangler::saveImage(Document *doc) const
{
	QString filename = doc->currentFilename();
	if(filename.isEmpty() || !utils::isWritableFormat(filename)) {
		return saveImageAs(doc);
	} else if(confirmFlatten(doc, filename)) {
		doc->saveCanvasAs(filename);
		return filename;
	} else {
		return QString{};
	}
}

QString FileWrangler::saveImageAs(Document *doc) const
{
	QString selectedFilter;
	QString filename = showSaveFileDialog(
		tr("Save Image"), LastPath::IMAGE, ".ora",
		utils::FileFormatOption::SaveImages, &selectedFilter,
		doc->currentFilename());

	if(!filename.isEmpty() && confirmFlatten(doc, filename)) {
		doc->saveCanvasAs(filename);
		return filename;
	} else {
		return QString{};
	}
}

QString FileWrangler::savePreResetImageAs(
	Document *doc, const drawdance::CanvasState &canvasState) const
{
	QString selectedFilter;
	QString filename = showSaveFileDialog(
		tr("Save Pre-Reset Image"), LastPath::IMAGE, ".ora",
		utils::FileFormatOption::SaveImages, &selectedFilter,
		doc->currentFilename());

	if(!filename.isEmpty() && confirmFlatten(doc, filename)) {
		doc->saveCanvasStateAs(filename, canvasState, false);
		return filename;
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

	if(!filename.isEmpty() && confirmFlatten(doc, filename)) {
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

QString FileWrangler::getSaveGifPath() const
{
	return showSaveFileDialog(
		tr("Export Animated GIF"), LastPath::IMAGE, ".gif",
		utils::FileFormatOption::SaveGif);
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

#ifdef Q_OS_ANDROID
QString FileWrangler::getSaveLogFilePath() const
{
	return showSaveFileDialog(
		tr("Log File"), LastPath::TABLET_EVENT_LOG, ".txt",
		utils::FileFormatOption::SaveText);
}
#else
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

QString FileWrangler::getSaveBrushPackPath() const
{
	return showSaveFileDialog(
		tr("Export Brushes"), LastPath::BRUSH_PACK, ".zip",
		utils::FileFormatOption::SaveBrushPack);
}

QString FileWrangler::getSaveSessionBansPath() const
{
	return showSaveFileDialog(
		tr("Export Session Bans"), LastPath::SESSION_BANS, ".dpbans",
		utils::FileFormatOption::SaveSessionBans);
}


bool FileWrangler::confirmFlatten(Document *doc, QString &filename) const
{
#ifdef Q_OS_ANDROID
	// We're not allowed to change the file extension on Android, all we could
	// do at this point is continue or cancel. So we'll just keep going.
	Q_UNUSED(doc);
	Q_UNUSED(filename);
	return true;
#else
	// If the image can be flattened without losing any information, we don't
	// need to confirm anything.
	bool needsOra = !filename.endsWith(".ora", Qt::CaseInsensitive) &&
					doc->canvas()->paintEngine()->needsOpenRaster();
	if(!needsOra) {
		return true;
	}

	static QRegularExpression extRe =
		QRegularExpression(QStringLiteral("\\.(\\w+)\\z"));
	QRegularExpressionMatch match = extRe.match(filename);
	QString format = match.hasMatch() ? match.captured(1).toUpper() : QString();

	QMessageBox box(
		QMessageBox::Information, tr("Save Image"),
		filename.endsWith(".psd", Qt::CaseInsensitive)
			? tr("The PSD format lacks support for annotations, the animation "
				 "timeline and some blend modes. If you want those to be "
				 "retained properly, you must save an ORA file.")
			: tr("The selected format will save a merged image. If you want to "
				 "retain layers, annotations and the animation timeline, you "
				 "must save an ORA file."),
		QMessageBox::Cancel, parentWidget());

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
		replaceExtension(filename, ".ora");
		setLastPath(LastPath::IMAGE, filename);
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
	QRegularExpressionMatch match =
		QRegularExpression{"\\.\\w*\\z"}.match(filename);
	if(match.hasMatch()) {
		filename = filename.left(match.capturedStart()) + ext;
	} else {
		filename += ext;
	}
}

bool FileWrangler::needsOra(Document *doc)
{
	return doc->canvas()->paintEngine()->needsOpenRaster();
}

QString FileWrangler::getLastPath(LastPath type, const QString &ext)
{
	const auto paths = dpApp().settings().lastFileOpenPaths();
	QString filename = paths.value(getLastPathKey(type)).toString();
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
	auto &settings = dpApp().settings();
	auto paths = settings.lastFileOpenPaths();
	paths[getLastPathKey(type)] = path;
	settings.setLastFileOpenPaths(paths);
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
#ifdef Q_OS_ANDROID
	case LastPath::LOG_FILE:
		return QStringLiteral("logfile");
#endif
	}
	return QStringLiteral("unknown");
}

QString FileWrangler::getDefaultLastPath(LastPath type, const QString &ext)
{
	switch(type) {
	case LastPath::ANIMATION_FRAMES:
		return QString{};
	case LastPath::DEBUG_DUMP:
		return utils::paths::writablePath("dumps");
	default:
		//: %1 will be a file extension, like .ora or .png or something.
		return tr("Untitled%1").arg(ext);
	}
}

QString FileWrangler::showOpenFileDialog(
	const QString &title, LastPath type, utils::FileFormatOptions formats) const
{
	QString filename = QFileDialog::getOpenFileName(
		parentWidget(), title, getLastPath(type),
		utils::fileFormatFilter(formats));
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
	std::optional<QString> lastPath) const
{
	QFileDialog fileDialog;
	fileDialog.setAcceptMode(QFileDialog::AcceptSave);

#ifdef Q_OS_ANDROID
	Q_UNUSED(title);
	Q_UNUSED(ext);

	// File handling on Android is all kinds of terrible. We can only use
	// the path that the user gave us, but have no way to restrict them to
	// sensible file extensions. If they don't provide a proper extension,
	// we aren't allowed to change it. To make matters worse, it will
	// automatically create a file of zero size that we can't delete. So we
	// ask the user for a name and file type first, using that as the
	// initial name and hope that most users will intuitively just pick a
	// directory to save the file in, rather than decide to break the path
	// after the fact.
	dialogs::AndroidFileDialog nameAndTypeDialog{
		lastPath.has_value() ? lastPath.value() : getLastPath(type),
		utils::fileFormatFilterList(formats), parentWidget()};
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
#else
	fileDialog.setWindowTitle(title);
	fileDialog.setNameFilters(utils::fileFormatFilterList(formats));
	if(selectedFilter && !selectedFilter->isEmpty()) {
		fileDialog.selectNameFilter(*selectedFilter);
	}
	fileDialog.selectFile(
		lastPath.has_value() ? lastPath.value() : getLastPath(type, ext));
#endif

	QString filename;
	do {
		bool ok = fileDialog.exec() == QDialog::Accepted &&
				  !fileDialog.selectedFiles().isEmpty();
		if(!ok) {
			return QString{};
		}

		filename = fileDialog.selectedFiles().first().trimmed();
		if(filename.isEmpty()) {
			return QString{};
		}

		bool emptySuffix = QFileInfo{filename}.completeSuffix().isEmpty();
		if(emptySuffix) {
#ifdef Q_OS_ANDROID
			// We're not allowed to change the suffix on Android, so we
			// don't have a way to proceed here other than telling the user
			// to please provide a proper name. To make matters worse, this
			// creates an empty file without an extension, which apparently
			// we can't delete either (attempted to do so through Qt,
			// java.io.File::delete and
			// android.content.ContentResolver::delete.)
			QMessageBox::critical(
				parentWidget(), tr("Missing File Extension"),
				tr("The file name you gave does not end with '%1' and "
				   "could "
				   "not be saved. Please provide a name with an extension.")
					.arg(selectedExt));
			filename = QString{};
#else
			filename += guessExtension(fileDialog.selectedNameFilter(), ext);
#endif
		}
	} while(filename.isEmpty());

#ifdef Q_OS_ANDROID
	setLastPath(type, initialFilename);
#else
	setLastPath(type, filename);
	if(selectedFilter) {
		*selectedFilter = fileDialog.selectedNameFilter();
	}
#endif

	return filename;
}

QWidget *FileWrangler::parentWidget() const
{
	return qobject_cast<QWidget *>(parent());
}
