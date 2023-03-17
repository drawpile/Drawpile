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
#ifndef ABUSEREPORTDIALOG_H
#define ABUSEREPORTDIALOG_H

#include <QDialog>

class Ui_AbuseReportDialog;

namespace dialogs {

class AbuseReportDialog final : public QDialog
{
	Q_OBJECT
public:
	explicit AbuseReportDialog(QWidget *parent=nullptr);
	~AbuseReportDialog() override;

	/**
	 * @brief Set the info about the session
	 * @param id
	 * @param alias
	 * @param title
	 */
	void setSessionInfo(const QString &id, const QString &alias, const QString &title);

	/**
	 * @brief Add a user to the selection box
	 *
	 * @param id
	 * @param name
	 */
	void addUser(int id, const QString &name);

	/**
	 * @brief Get the ID of the selected user.
	 *
	 * If no user was selected, the report is about the session in
	 * general.
	 * @return user Id or 0 if none was selected
	 */
	int userId() const;

	//! Get the reason message
	QString message() const;

private:
	Ui_AbuseReportDialog *m_ui;
	QString m_sessionId;
	int m_userId;
};

}

#endif

