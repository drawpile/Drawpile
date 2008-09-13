/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <QSettings>

#include "main.h"
#include "settingsdialog.h"

#include "ui_settings.h"
#include "../shared/net/constants.h"

namespace dialogs {

SettingsDialog::SettingsDialog(QWidget *parent)
	: QDialog(parent)
{
	ui_ = new Ui_SettingsDialog;
	ui_->setupUi(this);

	connect(ui_->buttonBox, SIGNAL(accepted()), this, SLOT(rememberSettings()));

	// Set defaults
	QSettings& cfg = DrawPileApp::getSettings();
	cfg.beginGroup("settings/server");
	ui_->multiconnect->setChecked(cfg.value("multiconnect",true).toBool());
	ui_->serverport->setValue(cfg.value("port",protocol::DEFAULT_PORT).toInt());
	ui_->maxnamelength->setValue(cfg.value("maxnamelength",16).toInt());
}

SettingsDialog::~SettingsDialog()
{
	delete ui_;
}

void SettingsDialog::rememberSettings() const
{
	QSettings& cfg = DrawPileApp::getSettings();
	cfg.beginGroup("settings/server");
	cfg.setValue("multiconnect", ui_->multiconnect->isChecked());
	if(ui_->serverport->value() == protocol::DEFAULT_PORT)
		cfg.remove("port");
	else
		cfg.setValue("port", ui_->serverport->value());
	cfg.setValue("maxnamelength", ui_->maxnamelength->value());
}

}

