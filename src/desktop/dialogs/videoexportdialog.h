/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef VIDEOEXPORTDIALOG_H
#define VIDEOEXPORTDIALOG_H

#include <QDialog>

class VideoExporter;
class Ui_VideoExport;

namespace dialogs {

class VideoExportDialog : public QDialog
{
	Q_OBJECT
public:
	explicit VideoExportDialog(QWidget *parent = 0);
	~VideoExportDialog();

	/**
	 * @brief Show the settings related to animation export
	 */
	void showAnimationSettings(int layers);

	/**
	 * @brief Get the new video exporter configured in this dialog
	 * @return exporter or 0 if non was configured
	 */
	VideoExporter *getExporter();

	// Animation settings

	//! Get the starting layer (animation)
	int getFirstLayer() const;

	//! Get the ending layer (animation)
	int getLastLayer() const;

private slots:
	void selectContainerFormat(const QString &fmt);

private:
	VideoExporter *getImageSeriesExporter();
	VideoExporter *getFfmpegExporter();
	VideoExporter *getGifExporter();

	Ui_VideoExport *_ui;
	QString _lastpath;
};

}

#endif // VIDEOEXPORTDIALOG_H
