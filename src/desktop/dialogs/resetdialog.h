/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016-2019 Calle Laakkonen

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

#include "../../libshared/net/message.h"

#include <QDialog>

namespace canvas {
	class StateTracker;
	class CanvasModel;
}

namespace dialogs {

class ResetDialog : public QDialog
{
	Q_OBJECT
public:
	explicit ResetDialog(const canvas::StateTracker *state, QWidget *parent=nullptr);
	~ResetDialog();

	protocol::MessageList resetImage(int myId, const canvas::CanvasModel *canvas);

private slots:
	void onPrevClick();
	void onNextClick();
	void onOpenClick();

private:
	struct Private;
	Private *d;
};

}

#endif

