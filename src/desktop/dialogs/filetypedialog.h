// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_FILETYPEDIALOG_H
#define DESKTOP_DIALOGS_FILETYPEDIALOG_H
#include "ui_filetypedialog.h"
#include <QDialog>

namespace dialogs {

class FileTypeDialog final : public QDialog {
	Q_OBJECT
public:
	explicit FileTypeDialog(
		const QString &name, const QStringList &formats,
		QWidget *parent = nullptr);

	QString name() const;
	QString type() const;

private slots:
	void updateUi();

private:
	Ui::FileTypeDialog m_ui;
};

}

#endif
