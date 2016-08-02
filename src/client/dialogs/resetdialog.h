/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016 Calle Laakkonen

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
#ifndef RESETSESSIONDIALOG_H
#define RESETSESSIONDIALOG_H

#include <QDialog>

namespace canvas {
	class StateTracker;
	class StateSavepoint;
}

namespace dialogs {

class ResetDialog : public QDialog
{
	Q_OBJECT
public:
	explicit ResetDialog(const canvas::StateTracker *state, QWidget *parent=0);
	~ResetDialog();

	canvas::StateSavepoint selectedSavepoint() const;

private slots:
	void onPrevClick();
	void onNextClick();

private:
	struct Private;
	Private *d;
};

}

#endif

