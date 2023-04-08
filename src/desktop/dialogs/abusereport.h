// SPDX-License-Identifier: GPL-3.0-or-later

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

