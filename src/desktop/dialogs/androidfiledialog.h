#ifndef ANDROIDFILEDIALOG_H
#define ANDROIDFILEDIALOG_H

#include "ui_androidfiledialog.h"
#include <QDialog>

namespace dialogs {

class AndroidFileDialog : public QDialog {
	Q_OBJECT
public:
	explicit AndroidFileDialog(
		const QString &name, const QStringList &formats,
		QWidget *parent = nullptr);

	~AndroidFileDialog();

	QString name() const;
	QString type() const;

private slots:
	void updateUi();

private:
	Ui::AndroidFileDialog m_ui;
};

}

#endif
