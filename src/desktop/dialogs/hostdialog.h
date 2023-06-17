// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef HOSTDIALOG_H
#define HOSTDIALOG_H

#include <QDialog>

class Ui_HostDialog;

namespace dialogs {

class HostDialog final : public QDialog {
	Q_OBJECT
public:
	explicit HostDialog(QWidget *parent = nullptr);
	~HostDialog() override;

	//! Store settings in configuration file
	void rememberSettings() const;

	//! Get the remote host address
	//! Returns an empty string if remote address is not used
	QString getRemoteAddress() const;

	//! Get session title
	QString getTitle() const;

	//! Get session password
	QString getPassword() const;

	//! Get the desired session alias
	QString getSessionAlias() const;

	//! Should the session have its NSFM flag set?
	bool isNsfm() const;

	//! Get the announcement server URL (empty if not selected)
	QString getAnnouncementUrl() const;

private slots:
	void updateNsfmBasedOnTitle();

private:
	Ui_HostDialog *m_ui;
	bool m_allowNsfm;
};

}

#endif
