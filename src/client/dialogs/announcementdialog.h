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

#ifndef ANNOUNCEMENTLISTDIALOG_H
#define ANNOUNCEMENTLISTDIALOG_H

#include <QDialog>

class QStringListModel;

class Ui_AnnouncementDialog;

namespace dialogs {

class AnnouncementDialog : public QDialog
{
	Q_OBJECT
public:
	AnnouncementDialog(QStringListModel *model, bool isAdmin, QWidget *parent=nullptr);
	~AnnouncementDialog();

signals:
	void requestAnnouncement(const QString &apiUrl);
	void requestUnlisting(const QString &apiUrl);

private:
	Ui_AnnouncementDialog *m_ui;
};

}

#endif
