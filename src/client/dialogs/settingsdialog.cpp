/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QSettings>
#include <QMessageBox>
#include <QHeaderView>
#include <QStyledItemDelegate>
#include <QItemEditorFactory>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QSslCertificate>

#include <QDebug>

#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
#include <QKeySequenceEdit>
#endif

#include "config.h"
#include "main.h"
#include "dialogs/settingsdialog.h"
#include "dialogs/certificateview.h"
#include "export/ffmpegexporter.h" // for setting ffmpeg path

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
	connect(_ui->buttonBox, SIGNAL(accepted()), this, SLOT(saveCertTrustChanges()));

	connect(_ui->pickFfmpeg, &QToolButton::clicked, [this]() {
		QString path = QFileDialog::getOpenFileName(this, tr("Set ffmepg path"), _ui->ffmpegpath->text(),
#ifdef Q_OS_WIN
			tr("Executables (*.exe)") + ";;" +
#endif
			tr("All files (*)")
		);
		if(!path.isEmpty())
			_ui->ffmpegpath->setText(path);
	});

	// Set defaults
	QSettings cfg;

	cfg.beginGroup("settings/recording");
	_ui->recordpause->setChecked(cfg.value("recordpause", true).toBool());
	_ui->minimumpause->setValue(cfg.value("minimumpause", 0.5).toFloat());
	_ui->ffmpegpath->setText(FfmpegExporter::getFfmpegPath());
	cfg.endGroup();

	cfg.beginGroup("settings/server");
	_ui->serverport->setValue(cfg.value("port",DRAWPILE_PROTO_DEFAULT_PORT).toInt());
	_ui->historylimit->setValue(cfg.value("historylimit", 0).toDouble());
	cfg.endGroup();

	cfg.beginGroup("settings/lag");
	_ui->strokepreview->setCurrentIndex(cfg.value("previewstyle", 2).toInt());
	cfg.endGroup();

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

	// Known hosts list
	connect(_ui->knownHostList, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(viewCertificate(QListWidgetItem*)));
	connect(_ui->knownHostList, SIGNAL(itemSelectionChanged()), this, SLOT(certificateSelectionChanged()));
	connect(_ui->trustKnownHosts, SIGNAL(clicked()), this, SLOT(markTrustedCertificates()));
	connect(_ui->removeKnownHosts, SIGNAL(clicked()), this, SLOT(removeCertificates()));
	connect(_ui->importTrustedButton, SIGNAL(clicked()), this, SLOT(importTrustedCertificate()));

	QStringList pemfilter; pemfilter << "*.pem";
	QDir knownHostsDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/known-hosts/");
	QIcon knownIcon = QIcon::fromTheme("security-medium", QIcon(":icons/security-medium"));

	for(const QString &filename : knownHostsDir.entryList(pemfilter, QDir::Files)) {
		auto *i = new QListWidgetItem(knownIcon, filename.left(filename.length()-4), _ui->knownHostList);
		i->setData(Qt::UserRole, false);
		i->setData(Qt::UserRole+1, knownHostsDir.absoluteFilePath(filename));
	}

	QDir trustedHostsDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/trusted-hosts/");
	QIcon trustedIcon = QIcon::fromTheme("security-high", QIcon(":icons/security-high"));
	for(const QString &filename : trustedHostsDir.entryList(pemfilter, QDir::Files)) {
		auto *i = new QListWidgetItem(trustedIcon, filename.left(filename.length()-4), _ui->knownHostList);
		i->setData(Qt::UserRole, true);
		i->setData(Qt::UserRole+1, trustedHostsDir.absoluteFilePath(filename));
	}
}

SettingsDialog::~SettingsDialog()
{
	delete _ui;
}

void SettingsDialog::rememberSettings()
{
	QSettings cfg;
	// Remember general settings
	cfg.beginGroup("settings/recording");
	cfg.setValue("recordpause", _ui->recordpause->isChecked());
	cfg.setValue("minimumpause", _ui->minimumpause->value());
	FfmpegExporter::setFfmpegPath(_ui->ffmpegpath->text().trimmed());
	cfg.endGroup();

	// Remember server settings
	cfg.beginGroup("settings/server");
	if(_ui->serverport->value() == DRAWPILE_PROTO_DEFAULT_PORT)
		cfg.remove("port");
	else
		cfg.setValue("port", _ui->serverport->value());

	cfg.setValue("historylimit", _ui->historylimit->value());

	cfg.endGroup();

	// Remember lag settings
	cfg.beginGroup("settings/lag");
	cfg.setValue("previewstyle", _ui->strokepreview->currentIndex());

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

	static_cast<DrawPileApp*>(qApp)->notifySettingsChanged();
}

void SettingsDialog::saveCertTrustChanges()
{
	// Delete removed certificates
	for(const QString &certfile : _removeCerts) {
		QFile(certfile).remove();
	}

	// Move selected certs to trusted certs
	QDir trustedDir = QDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/trusted-hosts/");
	trustedDir.mkpath(".");

	for(const QString &certfile : _trustCerts) {
		QString certname = certfile.mid(certfile.lastIndexOf('/')+1);
		QFile(certfile).rename(trustedDir.absoluteFilePath(certname));
	}

	// Save imported certificates
	for(const QSslCertificate &cert : _importCerts) {
		QString hostname = cert.subjectInfo(QSslCertificate::CommonName).at(0);

		QFile f(trustedDir.absoluteFilePath(hostname + ".pem"));
		if(!f.open(QFile::WriteOnly)) {
			qWarning() << "error opening" << f.fileName() << f.errorString();
			continue;
		}

		f.write(cert.toPem());
	}
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

void SettingsDialog::viewCertificate(QListWidgetItem *item)
{
	QString filename;
	if(item->data(Qt::UserRole+2).isNull())
		filename = item->data(Qt::UserRole+1).toString();
	else // read imported cert from original file
		filename = item->data(Qt::UserRole+2).toString();

	QList<QSslCertificate> certs = QSslCertificate::fromPath(filename);
	if(certs.isEmpty()) {
		qWarning() << "Certificate" << filename << "not found!";
		return;
	}

	CertificateView *cv = new CertificateView(item->text(), certs.at(0), this);
	cv->setAttribute(Qt::WA_DeleteOnClose);
	cv->show();
}

void SettingsDialog::certificateSelectionChanged()
{
	const QItemSelectionModel *sel = _ui->knownHostList->selectionModel();
	if(sel->selectedIndexes().isEmpty()) {
		_ui->trustKnownHosts->setEnabled(false);
		_ui->removeKnownHosts->setEnabled(false);
	} else {
		bool cantrust = false;
		for(const QModelIndex &idx : sel->selectedIndexes()) {
			if(!idx.data(Qt::UserRole).toBool()) {
				cantrust = true;
				break;
			}
		}
		_ui->trustKnownHosts->setEnabled(cantrust);
		_ui->removeKnownHosts->setEnabled(true);
	}
}

void SettingsDialog::markTrustedCertificates()
{
	QIcon trustedIcon = QIcon::fromTheme("security-high", QIcon(":icons/security-high"));
	for(QListWidgetItem *item : _ui->knownHostList->selectedItems()) {
		if(!item->data(Qt::UserRole).toBool()) {
			_trustCerts.append(item->data(Qt::UserRole+1).toString());
			item->setIcon(trustedIcon);
			item->setData(Qt::UserRole, true);
		}
	}
	_ui->trustKnownHosts->setEnabled(false);
}

void SettingsDialog::removeCertificates()
{
	for(QListWidgetItem *item : _ui->knownHostList->selectedItems()) {
		QString path = item->data(Qt::UserRole+1).toString();
		if(path.isEmpty()) {
			foreach(const QSslCertificate &imported, _importCerts) {
				if(imported.subjectInfo(QSslCertificate::CommonName).at(0) == item->text())
					_importCerts.removeOne(imported);
			}
		} else {
			_trustCerts.removeAll(path);
			_removeCerts.append(path);
		}

		delete item;
	}
}

void SettingsDialog::importTrustedCertificate()
{
	QString path = QFileDialog::getOpenFileName(this, tr("Import trusted certificate"), QString(),
		tr("Certificates (*.pem)") + ";;" +
		tr("All files (*)")
	);

	if(path.isEmpty())
		return;

	QList<QSslCertificate> certs = QSslCertificate::fromPath(path);
	if(certs.isEmpty() || certs.at(0).isNull()) {
		QMessageBox::warning(this, tr("Import trusted certificate"), tr("Invalid certificate!"));
		return;
	}

	if(certs.at(0).subjectInfo(QSslCertificate::CommonName).isEmpty()) {
		QMessageBox::warning(this, tr("Import trusted certificate"), tr("Certificate common name not set!"));
		return;
	}

	_importCerts.append(certs.at(0));

	QIcon trustedIcon = QIcon::fromTheme("security-high", QIcon(":icons/security-high"));
	auto *i = new QListWidgetItem(trustedIcon, certs.at(0).subjectInfo(QSslCertificate::CommonName).at(0), _ui->knownHostList);
	i->setData(Qt::UserRole, true);
	i->setData(Qt::UserRole+2, path);
}

}

