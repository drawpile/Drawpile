/*
   DrawPile - a collaborative drawing program.

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
#ifndef INDEXRECDIALOG_H
#define INDEXRECDIALOG_H

#include <QDialog>

#include "recording/index.h"

class Ui_FilterRecording;

namespace dialogs {

class FilterRecordingDialog : public QDialog
{
	Q_OBJECT
public:
	explicit FilterRecordingDialog(QWidget *parent = 0);
	~FilterRecordingDialog();

	void setSilence(const recording::IndexVector &silence);
	void setNewMarkers(const recording::IndexVector &markers);

	QString filterRecording(const QString &recordingFile);

private:
	Ui_FilterRecording *_ui;
	recording::IndexVector _silence;
	recording::IndexVector _newmarkers;
};

}

#endif
