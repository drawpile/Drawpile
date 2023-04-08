// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/newdialog.h"
#include "libclient/utils/images.h"

#include "ui_newdialog.h"

#include <QPushButton>
#include <QSettings>
#include <QMessageBox>

namespace dialogs {

NewDialog::NewDialog(QWidget *parent)
	: QDialog(parent)
{
	_ui = new Ui_NewDialog;
	_ui->setupUi(this);
	_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Create"));

	QSettings cfg;

	QSize lastSize = cfg.value("history/newsize", QSize(800, 600)).toSize();
	if(lastSize.isValid())
		setSize(lastSize);

	QColor lastColor = cfg.value("history/newcolor").value<QColor>();
	if(lastColor.isValid())
		setBackground(lastColor);
}

NewDialog::~NewDialog()
{
	delete _ui;
}

void NewDialog::setSize(const QSize &size)
{
	_ui->width->setValue(size.width());
	_ui->height->setValue(size.height());

}

void NewDialog::setBackground(const QColor &color)
{
	_ui->background->setColor(color);
}

void NewDialog::done(int r)
{
	if(r == QDialog::Accepted) {
		QSize size(_ui->width->value(), _ui->height->value());

		if(!utils::checkImageSize(size)) {
			QMessageBox::information(this, tr("Error"), tr("Size is too large"));
			return;

		} else {

			QSettings cfg;
			cfg.setValue("history/newsize", size);
			cfg.setValue("history/newcolor", _ui->background->color());
			emit accepted(size, _ui->background->color());

		}

	}

	QDialog::done(r);
}

}

