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
		&ShortcutFilterInput::handleFilterTextChanged);
	connect(
		m_conflictBox, &QCheckBox::stateChanged, this,
		&ShortcutFilterInput::handleConflictBoxStateChanged);
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

void ShortcutFilterInput::handleFilterTextChanged(const QString &text)
{
	if(!m_conflictBox->isChecked()) {
		updateFilterText(text);
	}
}

void ShortcutFilterInput::handleConflictBoxStateChanged(int state)
{
	bool checked = state != Qt::Unchecked;
	m_filterEdit->setDisabled(checked);
	updateFilterText(checked ? QStringLiteral("\1") : m_filterEdit->text());
	emit conflictBoxChecked(checked);
}

void ShortcutFilterInput::updateFilterText(const QString &text)
{
	QString filterText = text.trimmed();
	if(m_filterText != filterText) {
		m_filterText = filterText;
		emit filtered(m_filterText);
	}
}

}
}
