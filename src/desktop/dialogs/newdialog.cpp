// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/newdialog.h"
#include "desktop/main.h"
#include "libclient/utils/images.h"

#include "ui_newdialog.h"

#include <QPushButton>
#include <QMessageBox>

namespace dialogs {

NewDialog::NewDialog(QWidget *parent)
	: QDialog(parent)
{
	_ui = new Ui_NewDialog;
	_ui->setupUi(this);
	_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Create"));

	const auto &settings = dpApp().settings();
	const auto lastSize = settings.newCanvasSize();
	if(lastSize.isValid())
		setSize(lastSize);

	const auto lastColor = settings.newCanvasBackColor();
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
			auto &settings = dpApp().settings();
			settings.setNewCanvasSize(size);
			settings.setNewCanvasBackColor(_ui->background->color());
			emit accepted(size, _ui->background->color());
		}
	}

	QDialog::done(r);
}

}
