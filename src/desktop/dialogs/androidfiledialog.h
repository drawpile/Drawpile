// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ANDROIDFILEDIALOG_H
#define ANDROIDFILEDIALOG_H

#include "ui_androidfiledialog.h"
#include <QDialog>

namespace dialogs {

class AndroidFileDialog final : public QDialog {
	Q_OBJECT
public:
	explicit AndroidFileDialog(
		const QString &name, const QStringList &formats,
		QWidget *parent = nullptr);

	QString name() const;
	QString type() const;

private slots:
	void updateUi();

private:
	Ui::AndroidFileDialog m_ui;
};

}

#endif
