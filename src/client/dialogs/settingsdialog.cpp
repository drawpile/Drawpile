/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2008 Calle Laakkonen

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
#include <QMessageBox>
#include <QHeaderView>

#include "main.h"
#include "settingsdialog.h"

#include "ui_settings.h"
#include "../shared/net/constants.h"

namespace dialogs {

/**
 * Construct a settings dialog. The actions in the list should have
 * a "defaultshortcut" property for reset to default to work.
 *
 * @param actions list of customizeable actions (for shortcut editing)
 * @param parent parent widget
 */
SettingsDialog::SettingsDialog(const QList<QAction*>& actions, QWidget *parent)
	: QDialog(parent), acts_(actions)
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

	// Generate an editable list of shortcuts
	ui_->shortcuts->verticalHeader()->setVisible(false);
	ui_->shortcuts->setRowCount(acts_.size());
	for(int i=0;i<acts_.size();++i) {
		QTableWidgetItem *label = new QTableWidgetItem(acts_[i]->text().remove('&'));
		label->setFlags(Qt::ItemIsSelectable);
		const QKeySequence& defks = acts_[i]->property("defaultshortcut").value<QKeySequence>();
		if(acts_[i]->shortcut() != defks) {
			QFont font = label->font();
			font.setBold(true);
			label->setFont(font);
		}
		QTableWidgetItem *accel = new QTableWidgetItem(acts_[i]->shortcut().toString());
		QTableWidgetItem *def = new QTableWidgetItem(defks.toString());
		def->setFlags(Qt::ItemIsSelectable);
		ui_->shortcuts->setItem(i, 0, label);
		ui_->shortcuts->setItem(i, 1, accel);
		ui_->shortcuts->setItem(i, 2, def);
	}
	ui_->shortcuts->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
	ui_->shortcuts->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
	ui_->shortcuts->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
	connect(ui_->shortcuts, SIGNAL(cellChanged(int, int)),
			this, SLOT(validateShortcut(int, int)));
}

SettingsDialog::~SettingsDialog()
{
	delete ui_;
}

void SettingsDialog::rememberSettings() const
{
	QSettings& cfg = DrawPileApp::getSettings();
	// Remember server settings
	cfg.beginGroup("settings/server");
	cfg.setValue("multiconnect", ui_->multiconnect->isChecked());
	if(ui_->serverport->value() == protocol::DEFAULT_PORT)
		cfg.remove("port");
	else
		cfg.setValue("port", ui_->serverport->value());
	cfg.setValue("maxnamelength", ui_->maxnamelength->value());
	cfg.endGroup();

	// Remember shortcuts. Only shortcuts that have been changed
	// from their default values are stored.
	cfg.beginGroup("settings/shortcuts");
	cfg.remove("");
	bool changed = false;
	for(int i=0;i<acts_.size();++i) {
		QKeySequence ks(ui_->shortcuts->item(i, 1)->text());
		if(changed==false && ks != acts_[i]->shortcut())
			changed = true;
		if(ks != acts_[i]->property("defaultshortcut").value<QKeySequence>())
			cfg.setValue(acts_[i]->objectName(), ks);
	}
	if(changed)
		emit shortcutsChanged();
}

/**
 * Check that the new shortcut is valid
 */
void SettingsDialog::validateShortcut(int row, int col)
{
	// Check changes to shortcut column only
	if(col!=1)
		return;

	QString newShortcut = ui_->shortcuts->item(row, col)->text();
	QKeySequence ks(newShortcut);
	if(ks.isEmpty() && !newShortcut.isEmpty()) {
		// If new shortcut was invalid, restore the original
		ui_->shortcuts->setItem(row, col,
				new QTableWidgetItem(acts_[row]->shortcut().toString()));
	} else {
		// Check for conflicts.
		if(!ks.isEmpty()) {
			for(int c=0;c<acts_.size();++c) {
				if(c!=row && ks == QKeySequence(ui_->shortcuts->item(c, 1)->text())) {
					ui_->shortcuts->setItem(row, col,
						new QTableWidgetItem(acts_[row]->shortcut().toString()));
					QMessageBox::information(this, tr("Conflict"), tr("This shortcut is already used for \"%1\"").arg(acts_[c]->text().remove('&')));
					return;
				}
			}
		}
		// If the new shortcut is not the same as the default, make the
		// action label bold.
		QFont font = ui_->shortcuts->item(row, 0)->font();
		font.setBold(ks != acts_[row]->property("defaultshortcut").value<QKeySequence>());
		ui_->shortcuts->item(row, 0)->setFont(font);
	}
}

}

