// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef VIDEOEXPORTDIALOG_H
#define VIDEOEXPORTDIALOG_H

#include "libclient/export/videoexporter.h"
#include <QDialog>

class Ui_VideoExport;

namespace dialogs {

class VideoExportDialog final : public QDialog
{
	Q_OBJECT
public:
	explicit VideoExportDialog(QWidget *parent=nullptr);
	~VideoExportDialog() override;

	/**
	 * @brief Get the new video exporter configured in this dialog
	 * @return exporter or nullptr if none was configured
	 */
	VideoExporter *getExporter();

private slots:
	void chooseFfmpegPath();
	void updateUi();

private:
	QString getFfmpegPath();
	VideoExporter *getImageSeriesExporter();
	VideoExporter *getFfmpegExporter(VideoExporter::Format format);

	Ui_VideoExport *m_ui;
};

}

#endif // VIDEOEXPORTDIALOG_H
