#include "desktop/dialogs/settingsdialog/shortcutfilterinput.h"
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLineEdit>

namespace dialogs {
namespace settingsdialog {

ShortcutFilterInput::ShortcutFilterInput(QWidget *parent)
	: QWidget(parent)
{
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	setContentsMargins(0, 0, 0, 0);

	m_filterEdit = new QLineEdit;
	m_filterEdit->setClearButtonEnabled(true);
	m_filterEdit->setPlaceholderText(tr("Search…"));
	m_filterEdit->addAction(
		QIcon::fromTheme("edit-find"), QLineEdit::LeadingPosition);
	layout->addWidget(m_filterEdit, 1);

	m_conflictBox = new QCheckBox(tr("Show conflicts only"));
	layout->addWidget(m_conflictBox);

	connect(
		m_filterEdit, &QLineEdit::textChanged, this,
		&ShortcutFilterInput::handleFilterTextChanged, Qt::QueuedConnection);
	connect(
		m_conflictBox, COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox), this,
		&ShortcutFilterInput::handleConflictBoxStateChanged,
		Qt::QueuedConnection);
	connect(
		this, &ShortcutFilterInput::updateRequested, this,
		&ShortcutFilterInput::handleUpdate, Qt::QueuedConnection);
}

bool ShortcutFilterInput::isEmpty() const
{
	return m_filterText.isEmpty();
}

void ShortcutFilterInput::checkConflictBox()
{
	if(!m_conflictBox->isChecked()) {
		m_conflictBox->click();
	}
}

void ShortcutFilterInput::handleFilterTextChanged()
{
	if(!m_conflictBox->isChecked()) {
		requestUpdate();
	}
}

void ShortcutFilterInput::handleConflictBoxStateChanged(
	compat::CheckBoxState state)
{
	bool checked = state != Qt::Unchecked;
	m_filterEdit->setDisabled(checked);
	requestUpdate();
}

void ShortcutFilterInput::requestUpdate()
{
	if(!m_updatePending) {
		m_updatePending = true;
		Q_EMIT updateRequested();
	}
}

void ShortcutFilterInput::handleUpdate()
{
	bool showConflicts = m_conflictBox->isChecked();
	QString filterText =
		showConflicts ? QStringLiteral("\1") : m_filterEdit->text().trimmed();

	if(filterText != m_filterText) {
		m_filterText = filterText;
		Q_EMIT filtered(filterText);
	}

	if(m_showConflicts != showConflicts) {
		m_showConflicts = showConflicts;
		Q_EMIT conflictBoxChecked(showConflicts);
	}

	m_updatePending = false;
}

}
}
