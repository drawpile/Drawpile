/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#ifndef AVATARIMPORT_H
#define AVATARIMPORT_H

#include <QDialog>

class Ui_AvatarImport;

class QImage;

namespace dialogs {

class AvatarImport : public QDialog
{
	Q_OBJECT
public:
	AvatarImport(const QImage &source, QWidget *parent=nullptr);
	~AvatarImport();

	QImage croppedAvatar() const;

private:
	Ui_AvatarImport *m_ui;
	QImage m_originalImage;
};

}

#endif
