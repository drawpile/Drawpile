// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SYSTEMINFODIALOG_H
#define DESKTOP_DIALOGS_SYSTEMINFODIALOG_H
#include <QDialog>

class QTextEdit;

namespace dialogs {

class SystemInfoDialog final : public QDialog {
	Q_OBJECT
public:
	SystemInfoDialog(QWidget *parent = nullptr);

private:
	QString getSystemInfo() const;
	static QString getCompileFeatures();
	static QString boolToEnabledDisabled(bool b);
	static QString boolToYesNo(bool b);

	void copyToClipboard();

	QTextEdit *m_textEdit;
};

}

#endif
