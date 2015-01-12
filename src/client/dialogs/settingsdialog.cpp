/*
   Drawpile - a collaborative drawing program.

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

#include "config.h"
#include "main.h"
#include "dialogs/settingsdialog.h"
#include "dialogs/certificateview.h"
#include "export/ffmpegexporter.h" // for setting ffmpeg path
#include "widgets/keysequenceedit.h"
#include "utils/icon.h"

#include "ui_settings.h"

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

#include <algorithm>

class KeySequenceTableItem : public QTableWidgetItem
{
public:
	static const int TYPE = QTableWidgetItem::UserType + 1;
	KeySequenceTableItem(const QString &name, const QKeySequence &keysequence)
		: QTableWidgetItem(TYPE), _name(name), _keysequence(keysequence)
	{}

	const QString &actionName() const { return _name; }

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
	QString _name;
	QKeySequence _keysequence;
};

class KeySequenceEditFactory : public QItemEditorCreatorBase
{
public:
	QWidget *createWidget(QWidget *parent) const
	{
		return new widgets::KeySequenceEdit(parent);
	}

	QByteArray valuePropertyName() const
	{
		return "keySequence";
	}
};

namespace dialogs {

QMap<QString, SettingsDialog::CustomAction> SettingsDialog::_customizableActions;

/**
 * Construct a settings dialog. The actions in the list should have
 * a "defaultshortcut" property for reset to default to work.
 *
 * @param actions list of customizeable actions (for shortcut editing)
 * @param parent parent widget
 */
SettingsDialog::SettingsDialog(QWidget *parent)
	: QDialog(parent)
{
	_ui = new Ui_SettingsDialog;
	_ui->setupUi(this);

	connect(_ui->buttonBox, SIGNAL(accepted()), this, SLOT(rememberSettings()));
	connect(_ui->buttonBox, SIGNAL(accepted()), this, SLOT(saveCertTrustChanges()));

	connect(_ui->pickFfmpeg, &QToolButton::clicked, [this]() {
		QString path = QFileDialog::getOpenFileName(this, tr("Set ffmepg path"), _ui->ffmpegpath->text(),
#ifdef Q_OS_WIN
			tr("Executables (%1)").arg("*.exe") + ";;" +
#endif
			QApplication::tr("All files (*)")
		);
		if(!path.isEmpty())
			_ui->ffmpegpath->setText(path);
	});

	// Set defaults
	QSettings cfg;

	cfg.beginGroup("settings/input");
	_ui->tabletSupport->setChecked(cfg.value("tabletevents", true).toBool());
	cfg.endGroup();

	cfg.beginGroup("settings/recording");
	_ui->recordpause->setChecked(cfg.value("recordpause", true).toBool());
	_ui->minimumpause->setValue(cfg.value("minimumpause", 0.5).toFloat());
	_ui->ffmpegpath->setText(FfmpegExporter::getFfmpegPath());
	cfg.endGroup();

	cfg.beginGroup("settings/server");
	_ui->serverport->setValue(cfg.value("port",DRAWPILE_PROTO_DEFAULT_PORT).toInt());
	_ui->historylimit->setValue(cfg.value("historylimit", 0).toDouble());
	_ui->connTimeout->setValue(cfg.value("timeout", 60).toInt());
	cfg.endGroup();

	// Generate an editable list of shortcuts
	_ui->shortcuts->verticalHeader()->setVisible(false);
	_ui->shortcuts->setRowCount(_customizableActions.size());

	// QKeySequence editor delegate
	QStyledItemDelegate *keyseqdel = new QStyledItemDelegate(_ui->shortcuts);
	QItemEditorFactory *itemeditorfactory = new QItemEditorFactory;
	itemeditorfactory->registerEditor(QVariant::nameToType("QKeySequence"), new KeySequenceEditFactory);
	keyseqdel->setItemEditorFactory(itemeditorfactory);
	_ui->shortcuts->setItemDelegateForColumn(1, keyseqdel);

	QList<CustomAction> actions = getCustomizableActions();
	for(int i=0;i<actions.size();++i) {
		QTableWidgetItem *label = new QTableWidgetItem(actions[i].title);
		label->setFlags(Qt::ItemIsSelectable);
		if(actions[i].currentShortcut != actions[i].defaultShortcut) {
			QFont font = label->font();
			font.setBold(true);
			label->setFont(font);
		}
		QTableWidgetItem *accel = new KeySequenceTableItem(actions[i].name, actions[i].currentShortcut);
		QTableWidgetItem *def = new QTableWidgetItem(actions[i].defaultShortcut.toString());
		def->setFlags(Qt::ItemIsSelectable);
		_ui->shortcuts->setItem(i, 0, label);
		_ui->shortcuts->setItem(i, 1, accel);
		_ui->shortcuts->setItem(i, 2, def);
	}
	_ui->shortcuts->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);
	_ui->shortcuts->horizontalHeader()->setSectionResizeMode(1,QHeaderView::ResizeToContents);
	_ui->shortcuts->horizontalHeader()->setSectionResizeMode(2,QHeaderView::ResizeToContents);

	// TODO this signal doesn't seem to get emitted.
	connect(_ui->shortcuts, &QTableWidget::cellChanged, this, &SettingsDialog::validateShortcut);

	// Known hosts list
	connect(_ui->knownHostList, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(viewCertificate(QListWidgetItem*)));
	connect(_ui->knownHostList, SIGNAL(itemSelectionChanged()), this, SLOT(certificateSelectionChanged()));
	connect(_ui->trustKnownHosts, SIGNAL(clicked()), this, SLOT(markTrustedCertificates()));
	connect(_ui->removeKnownHosts, SIGNAL(clicked()), this, SLOT(removeCertificates()));
	connect(_ui->importTrustedButton, SIGNAL(clicked()), this, SLOT(importTrustedCertificate()));

	QStringList pemfilter; pemfilter << "*.pem";
	QDir knownHostsDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/known-hosts/");
	QIcon knownIcon = icon::fromTheme("security-medium");

	for(const QString &filename : knownHostsDir.entryList(pemfilter, QDir::Files)) {
		auto *i = new QListWidgetItem(knownIcon, filename.left(filename.length()-4), _ui->knownHostList);
		i->setData(Qt::UserRole, false);
		i->setData(Qt::UserRole+1, knownHostsDir.absoluteFilePath(filename));
	}

	QDir trustedHostsDir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/trusted-hosts/");
	QIcon trustedIcon = icon::fromTheme("security-high");
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

void SettingsDialog::registerCustomizableAction(const QString &name, const QString &title, const QKeySequence &defaultShortcut)
{
	if(_customizableActions.contains(name))
		return;

	_customizableActions[name] = CustomAction(name, title, defaultShortcut);
}

QList<SettingsDialog::CustomAction> SettingsDialog::getCustomizableActions()
{
	QList<CustomAction> actions;
	actions.reserve(_customizableActions.size());

	QSettings cfg;
	cfg.beginGroup("settings/shortcuts");

	for(CustomAction a : _customizableActions) {
		Q_ASSERT(!a.name.isEmpty());
		if(cfg.contains(a.name))
			a.currentShortcut = cfg.value(a.name).value<QKeySequence>();
		if(a.currentShortcut.isEmpty())
			a.currentShortcut = a.defaultShortcut;

		actions.append(a);
	}

	std::sort(actions.begin(), actions.end(),
		[](const CustomAction &a1, const CustomAction &a2) { return a1.title.compare(a2.title) < 0; }
	);

	return actions;
}

void SettingsDialog::rememberSettings()
{
	QSettings cfg;
	// Remember general settings
	cfg.beginGroup("settings/input");
	cfg.setValue("tabletevents", _ui->tabletSupport->isChecked());
	cfg.endGroup();

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
	cfg.setValue("timeout", _ui->connTimeout->value());

	cfg.endGroup();

	// Remember shortcuts.
	cfg.beginGroup("settings/shortcuts");
	cfg.remove("");

	for(int i=0;i<_ui->shortcuts->rowCount();++i) {
		KeySequenceTableItem *item = static_cast<KeySequenceTableItem*>(_ui->shortcuts->item(i, 1));
		QTableWidgetItem *itemDefault = static_cast<QTableWidgetItem*>(_ui->shortcuts->item(i, 2));
		Q_ASSERT(_customizableActions.contains(item->actionName()));

		const CustomAction &ca = _customizableActions.value(item->actionName());
		QKeySequence ks = item->data(Qt::EditRole).value<QKeySequence>();

		if(ks.isEmpty() || ks.toString() == itemDefault->data(Qt::DisplayRole))
			cfg.remove(ca.name);
		else
			cfg.setValue(ca.name, ks);
	}

	static_cast<DrawpileApp*>(qApp)->notifySettingsChanged();
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
	// TODO for some reason this function is never called
	// TODO check for conflicting shortcuts here.
	qDebug() << "validateShortcut" << row << col;
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
	QIcon trustedIcon = icon::fromTheme("security-high");
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
		tr("Certificates (%1)").arg("*.pem *.crt *.cer") + ";;" +
		QApplication::tr("All files (*)")
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

	QIcon trustedIcon = icon::fromTheme("security-high");
	auto *i = new QListWidgetItem(trustedIcon, certs.at(0).subjectInfo(QSslCertificate::CommonName).at(0), _ui->knownHostList);
	i->setData(Qt::UserRole, true);
	i->setData(Qt::UserRole+2, path);
}

}

