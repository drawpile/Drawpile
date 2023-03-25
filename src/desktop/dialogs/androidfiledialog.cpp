#include "desktop/dialogs/androidfiledialog.h"
#include <QPushButton>

namespace dialogs {

AndroidFileDialog::AndroidFileDialog(
	const QString &name, const QStringList &formats, QWidget *parent)
	: QDialog{parent}
{
	m_ui.setupUi(this);
	m_ui.nameEdit->setText(name);
	m_ui.typeList->addItems(formats);
	m_ui.typeList->setCurrentRow(0);
	connect(
		m_ui.nameEdit, &QLineEdit::textChanged, this,
		&AndroidFileDialog::updateUi);
	connect(
		m_ui.typeList, &QListWidget::itemSelectionChanged, this,
		&AndroidFileDialog::updateUi);
	connect(m_ui.buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(m_ui.buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	updateUi();
}

QString AndroidFileDialog::name() const
{
	return m_ui.nameEdit->text().trimmed();
}

QString AndroidFileDialog::type() const
{
	QList<QListWidgetItem *> selected = m_ui.typeList->selectedItems();
	if(selected.isEmpty()) {
		return QString{};
	} else {
		return selected.first()->text();
	}
}

void AndroidFileDialog::updateUi()
{
	QPushButton *okButton = m_ui.buttons->button(QDialogButtonBox::Ok);
	if(okButton) {
		okButton->setDisabled(
			m_ui.nameEdit->text().trimmed().isEmpty() ||
			m_ui.typeList->selectedItems().isEmpty());
	}
}

}
