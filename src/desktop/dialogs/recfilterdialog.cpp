/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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

#include "dialogs/recfilterdialog.h"
#include "recording/filter.h"

#include "ui_recfilter.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>

namespace dialogs {

FilterRecordingDialog::FilterRecordingDialog(QWidget *parent) :
	QDialog(parent)
{
	_ui = new Ui_FilterRecording;
	_ui->setupUi(this);
	_ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Filter"));
}

FilterRecordingDialog::~FilterRecordingDialog()
{
	delete _ui;
}

QString FilterRecordingDialog::filterRecording(const QString &recordingFile)
{
	QSettings cfg;

	// First, get output file name
	QString outfile = QFileDialog::getSaveFileName(this, tr("Save filtered recording"),
		cfg.value("window/lastpath").toString(),
		tr("Recordings (%1)").arg("*.dprec") + ";;" +
		tr("Compressed recordings (%1)").arg("*.dprecz")
	);

	if(outfile.isEmpty())
		return QString();

	cfg.setValue("window/lastpath", outfile);

	if(!outfile.endsWith(".dprec", Qt::CaseInsensitive))
		outfile.append(".dprec");

	// Set filtering options
	const recording::FilterOptions filter {
		_ui->removeUndos->isChecked(),
		_ui->removeChat->isChecked(),
		_ui->removeLookyloos->isChecked(),
		_ui->removeDelays->isChecked(),
		_ui->removeLasers->isChecked(),
		_ui->removeMarkers->isChecked(),
		_ui->squishStrokes->isChecked()
	};


	// Perform filtering
	QString error;
	if(!recording::filterRecording(recordingFile, outfile, filter, &error)) {
		QMessageBox::warning(this, tr("Error"), error);
		return QString();
	}

	return outfile;
}

}
