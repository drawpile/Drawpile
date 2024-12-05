// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/host/listing.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/utils/widgetutils.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {
namespace host {

Listing::Listing(QWidget *parent)
	: QWidget(parent)
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	m_automaticBox =
		new QCheckBox(tr("Set title and description automatically"));
	layout->addWidget(m_automaticBox);
	connect(
		m_automaticBox, COMPAT_CHECKBOX_STATE_CHANGED_SIGNAL(QCheckBox), this,
		&Listing::updateEditSectionVisibility);

	m_editSection = new QWidget;
	m_editSection->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(m_editSection);

	QFormLayout *manualLayout = new QFormLayout(m_editSection);
	manualLayout->setContentsMargins(0, 0, 0, 0);

	m_titleEdit = new QLineEdit;
	m_titleEdit->setMaxLength(50);
	m_titleEdit->setPlaceholderText(tr("Enter a publicly visible title"));
	manualLayout->addRow(tr("Title:"), m_titleEdit);

	m_descriptionEdit = new QPlainTextEdit;
	m_descriptionEdit->setPlaceholderText(tr(
		"Enter a description for the session, like rules or allowed/forbidden "
		"content. This is publicly visible, even for personal sessions!"));
	manualLayout->addRow(tr("Description:"), m_descriptionEdit);
	connect(
		m_descriptionEdit, &QPlainTextEdit::textChanged, this,
		&Listing::updateDescriptionLength);

	m_descriptionCounter = new QLabel;
	m_descriptionCounter->setAlignment(Qt::AlignRight);
	m_descriptionCounter->setTextFormat(Qt::RichText);
	manualLayout->addRow(m_descriptionCounter);

	utils::addFormSeparator(layout);

	QFormLayout *listingLayout = new QFormLayout;
	listingLayout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(listingLayout);

	m_listings = new QListView;
	listingLayout->addRow(tr("Announcements:"), m_listings);

	QHBoxLayout *buttonsLayout = new QHBoxLayout;
	buttonsLayout->setContentsMargins(0, 0, 0, 0);
	listingLayout->addRow(buttonsLayout);

	buttonsLayout->addStretch();

	m_addListingButton =
		new QPushButton(QIcon::fromTheme("list-add"), tr("Add"));
	m_addListingButton->setMenu(new QMenu(m_addListingButton));
	buttonsLayout->addWidget(m_addListingButton);

	m_removeListingButton =
		new QPushButton(QIcon::fromTheme("list-remove"), tr("Remove selected"));
	m_removeListingButton->setEnabled(false);
	buttonsLayout->addWidget(m_removeListingButton);

	updateEditSectionVisibility();
	updateDescriptionLength();
}

void Listing::setPersonal(bool personal)
{
	if(m_personal != personal) {
		m_personal = personal;
		m_automaticBox->setEnabled(m_personal);
		m_automaticBox->setVisible(m_personal);
		updateEditSectionVisibility();
	}
}

void Listing::updateEditSectionVisibility()
{
	bool visible = !m_personal || !m_automaticBox->isChecked();
	m_editSection->setVisible(visible);
}

void Listing::updateDescriptionLength()
{
	if(!m_updatingDescription) {
		QScopedValueRollback<bool> rollback(m_updatingDescription, true);
		int len = m_descriptionEdit->toPlainText().trimmed().length();
		QString text;
		if(len <= MAX_DESCRIPTION_LENGTH) {
			text = QStringLiteral("&nbsp;%1/%2&nbsp;")
					   .arg(len)
					   .arg(MAX_DESCRIPTION_LENGTH);
		} else {
			text = tr("<strong>%1</strong> - <span "
					  "style=\"background-color:#dc3545;color:#fff;\">&nbsp;%2/"
					  "%3&nbsp;</span>")
					   .arg(tr("Description too long!").toHtmlEscaped())
					   .arg(len)
					   .arg(MAX_DESCRIPTION_LENGTH);
		}
		m_descriptionCounter->setText(text);
	}
}

}
}
}
