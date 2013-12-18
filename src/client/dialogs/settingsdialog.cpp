/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2013 Calle Laakkonen

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
#include <QStyledItemDelegate>
#include <QItemEditorFactory>

#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
#include <QKeySequenceEdit>
#endif

#include <qdebug.h>

#include "config.h"
#include "settingsdialog.h"

#include "ui_settings.h"

class KeySequenceTableItem : public QTableWidgetItem
{
public:
	static const int TYPE = QTableWidgetItem::UserType + 1;
	KeySequenceTableItem(const QKeySequence &keysequence)
		: QTableWidgetItem(TYPE), _keysequence(keysequence)
	{}

	QVariant data(int role) const
	{
		switch(role) {
		case Qt::DisplayRole: return _keysequence.toString();
		case Qt::EditRole: return _keysequence;
		default: return QVariant();
		}
	}

	void setData(int role, const QVariant &data)
	{
		if(role == Qt::EditRole)
			_keysequence = data.value<QKeySequence>();
	}

private:
	QKeySequence _keysequence;
};

#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
class KeySequenceEditFactory : public QItemEditorCreatorBase
{
public:
	QWidget *createWidget(QWidget *parent) const
	{
		return new QKeySequenceEdit(parent);
	}

	QByteArray valuePropertyName() const
	{
		return "keySequence";
	}
};
#endif

namespace dialogs {

/**
 * Construct a settings dialog. The actions in the list should have
 * a "defaultshortcut" property for reset to default to work.
 *
 * @param actions list of customizeable actions (for shortcut editing)
 * @param parent parent widget
 */
SettingsDialog::SettingsDialog(const QList<QAction*>& actions, QWidget *parent)
	: QDialog(parent), _customactions(actions)
{
	_ui = new Ui_SettingsDialog;
	_ui->setupUi(this);

	connect(_ui->buttonBox, SIGNAL(accepted()), this, SLOT(rememberSettings()));

	// Set defaults
	QSettings cfg;
	cfg.beginGroup("settings/server");
	_ui->serverport->setValue(cfg.value("port",DRAWPILE_PROTO_DEFAULT_PORT).toInt());

	// Generate an editable list of shortcuts
	_ui->shortcuts->verticalHeader()->setVisible(false);
	_ui->shortcuts->setRowCount(_customactions.size());

	// QKeySequence editor delegate
#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
	QStyledItemDelegate *keyseqdel = new QStyledItemDelegate(_ui->shortcuts);
	QItemEditorFactory *itemeditorfactory = new QItemEditorFactory;
	itemeditorfactory->registerEditor(QVariant::nameToType("QKeySequence"), new KeySequenceEditFactory);
	keyseqdel->setItemEditorFactory(itemeditorfactory);
	_ui->shortcuts->setItemDelegateForColumn(1, keyseqdel);
#endif

	for(int i=0;i<_customactions.size();++i) {
		QTableWidgetItem *label = new QTableWidgetItem(_customactions[i]->text().remove('&'));
		label->setFlags(Qt::ItemIsSelectable);
		const QKeySequence& defks = _customactions[i]->property("defaultshortcut").value<QKeySequence>();
		if(_customactions[i]->shortcut() != defks) {
			QFont font = label->font();
			font.setBold(true);
			label->setFont(font);
		}
		QTableWidgetItem *accel = new KeySequenceTableItem(_customactions[i]->shortcut());
		QTableWidgetItem *def = new QTableWidgetItem(defks.toString());
		def->setFlags(Qt::ItemIsSelectable);
		_ui->shortcuts->setItem(i, 0, label);
		_ui->shortcuts->setItem(i, 1, accel);
		_ui->shortcuts->setItem(i, 2, def);
	}
	_ui->shortcuts->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
	_ui->shortcuts->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
	_ui->shortcuts->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);
	connect(_ui->shortcuts, SIGNAL(cellChanged(int, int)),
			this, SLOT(validateShortcut(int, int)));
}

SettingsDialog::~SettingsDialog()
{
	delete _ui;
}

void SettingsDialog::rememberSettings() const
{
	QSettings cfg;
	// Remember server settings
	cfg.beginGroup("settings/server");
	if(_ui->serverport->value() == DRAWPILE_PROTO_DEFAULT_PORT)
		cfg.remove("port");
	else
		cfg.setValue("port", _ui->serverport->value());
	cfg.endGroup();

	// Remember shortcuts. Only shortcuts that have been changed
	// from their default values are stored.
	cfg.beginGroup("settings/shortcuts");
	cfg.remove("");
	bool changed = false;
	for(int i=0;i<_customactions.size();++i) {
		QKeySequence ks(_ui->shortcuts->item(i, 1)->text());
		if(changed==false && ks != _customactions[i]->shortcut())
			changed = true;
		if(ks != _customactions[i]->property("defaultshortcut").value<QKeySequence>())
			cfg.setValue(_customactions[i]->objectName(), ks);
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

	QString newShortcut = _ui->shortcuts->item(row, col)->text();
	QKeySequence ks(newShortcut);
	if(ks.isEmpty() && !newShortcut.isEmpty()) {
		// If new shortcut was invalid, restore the original
		_ui->shortcuts->setItem(row, col,
				new QTableWidgetItem(_customactions[row]->shortcut().toString()));
	} else {
		// Check for conflicts.
		if(!ks.isEmpty()) {
			for(int c=0;c<_customactions.size();++c) {
				if(c!=row && ks == QKeySequence(_ui->shortcuts->item(c, 1)->text())) {
					_ui->shortcuts->setItem(row, col,
						new QTableWidgetItem(_customactions[row]->shortcut().toString()));
					QMessageBox::information(this, tr("Conflict"), tr("This shortcut is already used for \"%1\"").arg(_customactions[c]->text().remove('&')));
					return;
				}
			}
		}
		// If the new shortcut is not the same as the default, make the
		// action label bold.
		QFont font = _ui->shortcuts->item(row, 0)->font();
		font.setBold(ks != _customactions[row]->property("defaultshortcut").value<QKeySequence>());
		_ui->shortcuts->item(row, 0)->setFont(font);
	}
}

}

