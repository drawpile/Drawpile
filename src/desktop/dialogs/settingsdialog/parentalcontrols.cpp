// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/parentalcontrols.h"
#include "desktop/settings.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libshared/util/passwordhash.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace dialogs {
namespace settingsdialog {

ParentalControls::ParentalControls(
	desktop::settings::Settings &settings, QWidget *parent)
	: Page(parent)
	, m_settings(settings)
{
	init(settings, false);
}

void ParentalControls::setUp(
	desktop::settings::Settings &settings, QVBoxLayout *layout)
{
	initInfoBar(layout);

	if(parentalcontrols::isOSActive()) {
		initOsManaged(layout);
	} else {
		initBuiltIn(settings, layout);
	}
}

void ParentalControls::initBuiltIn(
	desktop::settings::Settings &settings, QVBoxLayout *layout)
{
	auto *stack = new QStackedWidget;
	stack->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(stack);

	auto *formPage = new QWidget;
	auto *form = new QFormLayout;
	formPage->setLayout(form);
	form->setContentsMargins(0, 0, 0, 0);
	form->setFormAlignment(Qt::AlignTop);
	stack->addWidget(formPage);

	auto *autoTag = new QCheckBox(
		tr("Consider sessions whose titles contain these keywords NSFM."));
	form->addRow(tr("Keywords:"), autoTag);
	settings.bindParentalControlsAutoTag(autoTag);
	auto *tagWords = new QPlainTextEdit(this);
	utils::bindKineticScrolling(tagWords);
	tagWords->setPlaceholderText(parentalcontrols::defaultWordList());
	form->addRow(nullptr, tagWords);
	settings.bindParentalControlsTags(tagWords);
	settings.bindParentalControlsAutoTag(tagWords, &QPlainTextEdit::setEnabled);

	utils::addFormSpacer(form);

	auto *level = utils::addRadioGroup(
		form, tr("Filter mode:"), false,
		{{tr("Unrestricted"), int(parentalcontrols::Level::Unrestricted)},
		 {tr("Remove NSFM sessions from listings"),
		  int(parentalcontrols::Level::NoList)},
		 {tr("Prevent joining NSFM sessions"),
		  int(parentalcontrols::Level::NoJoin)},
		 {tr("Prevent joining NSFM sessions and disconnect"),
		  int(parentalcontrols::Level::Restricted)}});
	level->button(1)->setToolTip(
		tr("NSFM sessions will be hidden from server "
		   "listings, but can still be joined by following a direct link."));
	level->button(2)->setToolTip(tr("NSFM sessions will be hidden from server "
									"listings and cannot be joined."));
	level->button(3)->setToolTip(
		tr("NSFM sessions will be hidden from server "
		   "listings and cannot be joined. Connected sessions that change "
		   "their title or NSFM flag will automatically disconnect."));
	settings.bindParentalControlsLevel(level);

	auto *forceCensor = new QCheckBox(tr("Disallow uncensoring of layers"));
	settings.bindParentalControlsForceCensor(forceCensor);
	form->addRow(nullptr, forceCensor);

	auto *warnOnJoin = new QCheckBox(tr("Warn when joining NSFM sessions"));
	settings.bindShowNsfmWarningOnJoin(warnOnJoin);
	form->addRow(nullptr, warnOnJoin);

	layout->addStretch();
	utils::addFormSeparator(layout);

	auto *lockLayout = new QHBoxLayout;
	lockLayout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(lockLayout);

	auto *lock = new QPushButton(tr("Lock"));
	lock->setAutoDefault(false);
	lockLayout->addWidget(lock);
	connect(lock, &QPushButton::clicked, this, &ParentalControls::toggleLock);

	auto *hide = new QCheckBox(tr("Hide settings when locked"));
	lockLayout->addWidget(hide);
	settings.bindParentalControlsHideLocked(hide);

	lockLayout->addStretch();

	auto *hiddenPage = new QWidget;
	auto *hiddenLayout = new QVBoxLayout(hiddenPage);
	hiddenLayout->setContentsMargins(0, 0, 0, 0);
	hiddenLayout->addWidget(
		new QLabel(tr("Parental controls are currently locked.")), 0,
		Qt::AlignCenter);
	stack->addWidget(hiddenPage);

	settings.bindParentalControlsLocked(this, [=, &settings](QByteArray hash) {
		const auto locked = !hash.isEmpty();
		tagWords->setDisabled(locked || !settings.parentalControlsAutoTag());
		autoTag->setDisabled(locked);
		for(auto *button : level->buttons()) {
			button->setDisabled(locked);
		}
		forceCensor->setDisabled(locked);
		hide->setDisabled(locked);
		lock->setText(locked ? tr("Unlock") : tr("Lock"));

		stack->setCurrentWidget(
			locked && settings.parentalControlsHideLocked() ? hiddenPage
															: formPage);
	});
}

void ParentalControls::initInfoBar(QVBoxLayout *layout)
{
	auto *info = new QHBoxLayout;
	info->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(info);

	info->addWidget(
		utils::makeIconLabel(QIcon::fromTheme("dialog-information"), this));

	auto *description = new QLabel;
	description->setWordWrap(true);
	description->setText(
		tr("These settings configure the handling of sessions that are marked "
		   "not suitable for minors (NSFM) and of layers that have been "
		   "censored."));

	info->addWidget(description);

	utils::addFormSeparator(layout);
}

void ParentalControls::initOsManaged(QVBoxLayout *layout)
{
	layout->addStretch();
	layout->addWidget(
		new QLabel(tr("Parental controls are currently managed by the "
					  "operating system.")),
		0, Qt::AlignCenter);
	layout->addStretch();
}

void ParentalControls::toggleLock()
{
	Qt::WindowFlags flags;
#ifndef __EMSCRIPTEN__
	flags.setFlag(Qt::Sheet);
#endif
	QInputDialog *dlg = new QInputDialog(this, flags);

	QByteArray hash = m_settings.parentalControlsLocked();
	bool locked = !hash.isEmpty();
	QString title =
		locked ? tr("Unlock Parental Controls") : tr("Lock Parental Controls");
	dlg->setWindowTitle(title);
	dlg->setLabelText(tr("Enter password:"));
	dlg->setInputMode(QInputDialog::TextInput);
	dlg->setTextEchoMode(QLineEdit::Password);
	connect(
		dlg, &QInputDialog::accepted, this, [this, dlg, hash, locked, title] {
			QString pass = dlg->textValue();
			if(!pass.isEmpty()) {
				if(locked) {
					if(server::passwordhash::check(pass, hash)) {
						m_settings.setParentalControlsLocked(QByteArray());
					} else {
						QMessageBox *box = utils::makeMessage(
							this, title, tr("Incorrect password."), QString(),
							QMessageBox::Warning,
							QMessageBox::Retry | QMessageBox::Cancel);
						connect(box, &QMessageBox::accepted, this, [this] {
							toggleLock();
						});
						box->show();
					}
				} else {
					m_settings.setParentalControlsLocked(
						server::passwordhash::hash(pass));
				}
			}
		});
	dlg->show();
}

} // namespace settingsdialog
} // namespace dialogs
