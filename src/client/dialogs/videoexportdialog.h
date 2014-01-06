/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
	 * @brief Get the new video exporter configured in this dialog
	 * @return exporter or 0 if non was configured
	 */
	VideoExporter *getExporter();

private slots:
	void selectOutputDirectory();

private:
	VideoExporter *getImageSeriesExporter();

	Ui_VideoExport *_ui;
};

}

#endif // VIDEOEXPORTDIALOG_H
