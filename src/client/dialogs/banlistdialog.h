/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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

#ifndef BANLISTDIALOG_H
#define BANLISTDIALOG_H

#include <QDialog>

class QAbstractButton;
class BanlistModel;

namespace dialogs {

class BanlistDialog : public QDialog
{
	Q_OBJECT
public:
	BanlistDialog(BanlistModel *model, bool isAdmin, QWidget *parent=nullptr);

signals:
	void requestBanRemoval(int entryId);
};

}

#endif // BANLISTDIALOG_H
