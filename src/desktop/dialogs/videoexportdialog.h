// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_VIDEOEXPORTDIALOG_H
#define DESKTOP_DIALOGS_VIDEOEXPORTDIALOG_H
#include "libclient/export/videoexporter.h"
#include "libclient/utils/debouncetimer.h"
#include <QDialog>

class Ui_VideoExport;

namespace dialogs {

class VideoExportDialog final : public QDialog {
	Q_OBJECT
public:
	explicit VideoExportDialog(QWidget *parent = nullptr);
	~VideoExportDialog() override;

	/**
	 * @brief Get the new video exporter configured in this dialog
	 * @return exporter or nullptr if none was configured
	 */
	VideoExporter *getExporter();

private slots:
	void checkIsFfmpegAvailable();
	void chooseFfmpegPath();
	void updateUi();
	void chooseExportPath();

private:
	enum class FfmpegError { Ok, Checking, Error };
	void setFfmpegError(FfmpegError error);
	QString getFfmpegPath();
	VideoExporter *getImageSeriesExporter();
	VideoExporter *getFfmpegExporter();

	Ui_VideoExport *m_ui;
	VideoExporter::Format m_format;
	QString m_exportPath;
	DebounceTimer m_ffmpegPathDebounce;
};

}

#endif // VIDEOEXPORTDIALOG_H
