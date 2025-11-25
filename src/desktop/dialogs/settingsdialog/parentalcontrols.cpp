// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/parentalcontrols.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/config/config.h"
#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libshared/util/passwordhash.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace dialogs {
namespace settingsdialog {

ParentalControls::ParentalControls(config::Config *cfg, QWidget *parent)
	: Page(parent)
	, m_cfg(cfg)
{
	init(cfg, false);
}

void ParentalControls::setUp(config::Config *cfg, QVBoxLayout *layout)
{
	initInfoBar(layout);

	if(parentalcontrols::isOSActive()) {
		initOsManaged(layout);
	} else {
		initBuiltIn(cfg, layout);
	}
}

void ParentalControls::initBuiltIn(config::Config *cfg, QVBoxLayout *layout)
{
	QStackedWidget *stack = new QStackedWidget;
	stack->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(stack);

	QWidget *formPage = new QWidget;
	QFormLayout *form = new QFormLayout;
	formPage->setLayout(form);
	form->setContentsMargins(0, 0, 0, 0);
	form->setFormAlignment(Qt::AlignTop);
	stack->addWidget(formPage);

	QCheckBox *autoTag = new QCheckBox(
		tr("Consider sessions whose titles contain these keywords NSFM."));
	form->addRow(tr("Keywords:"), autoTag);
	CFG_BIND_CHECKBOX(cfg, ParentalControlsAutoTag, autoTag);
	QPlainTextEdit *tagWords = new QPlainTextEdit(this);
	utils::bindKineticScrolling(tagWords);
	tagWords->setPlaceholderText(parentalcontrols::defaultWordList());
	form->addRow(nullptr, tagWords);
	CFG_BIND_PLAINTEXTEDIT(cfg, ParentalControlsTags, tagWords);
	CFG_BIND_SET(
		cfg, ParentalControlsAutoTag, tagWords, QPlainTextEdit::setEnabled);

	utils::addFormSpacer(form);

	QButtonGroup *level = utils::addRadioGroup(
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
	level->button(2)->setToolTip(
		tr("NSFM sessions will be hidden from server "
		   "listings and cannot be joined."));
	level->button(3)->setToolTip(
		tr("NSFM sessions will be hidden from server "
		   "listings and cannot be joined. Connected sessions that change "
		   "their title or NSFM flag will automatically disconnect."));
	CFG_BIND_BUTTONGROUP(cfg, ParentalControlsLevel, level);

	QCheckBox *forceCensor =
		new QCheckBox(tr("Disallow uncensoring of layers"));
	CFG_BIND_CHECKBOX(cfg, ParentalControlsForceCensor, forceCensor);
	form->addRow(nullptr, forceCensor);

	QCheckBox *warnOnJoin =
		new QCheckBox(tr("Warn when joining NSFM sessions"));
	CFG_BIND_CHECKBOX(cfg, ShowNsfmWarningOnJoin, warnOnJoin);
	form->addRow(nullptr, warnOnJoin);

	layout->addStretch();
	utils::addFormSeparator(layout);

	QHBoxLayout *lockLayout = new QHBoxLayout;
	lockLayout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(lockLayout);

	QPushButton *lock = new QPushButton(tr("Lock"));
	lock->setAutoDefault(false);
	lockLayout->addWidget(lock);
	connect(lock, &QPushButton::clicked, this, &ParentalControls::toggleLock);

	QCheckBox *hide = new QCheckBox(tr("Hide settings when locked"));
	lockLayout->addWidget(hide);
	CFG_BIND_CHECKBOX(cfg, ParentalControlsHideLocked, hide);

	lockLayout->addStretch();

	QWidget *hiddenPage = new QWidget;
	QVBoxLayout *hiddenLayout = new QVBoxLayout(hiddenPage);
	hiddenLayout->setContentsMargins(0, 0, 0, 0);
	hiddenLayout->addWidget(
		new QLabel(tr("Parental controls are currently locked.")), 0,
		Qt::AlignCenter);
	stack->addWidget(hiddenPage);

	CFG_BIND_SET_FN(
		cfg, ParentalControlsLocked, this, ([=](QByteArray hash) {
			bool locked = !hash.isEmpty();
			tagWords->setDisabled(locked || !cfg->getParentalControlsAutoTag());
			autoTag->setDisabled(locked);
			for(QAbstractButton *button : level->buttons()) {
				button->setDisabled(locked);
			}
			forceCensor->setDisabled(locked);
			hide->setDisabled(locked);
			lock->setText(locked ? tr("Unlock") : tr("Lock"));

			stack->setCurrentWidget(
				locked && cfg->getParentalControlsHideLocked() ? hiddenPage
															   : formPage);
		}));
}

void ParentalControls::initInfoBar(QVBoxLayout *layout)
{
	QHBoxLayout *info = new QHBoxLayout;
	info->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(info);

	info->addWidget(
		utils::makeIconLabel(QIcon::fromTheme("dialog-information"), this));

	QLabel *description = new QLabel;
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
		new QLabel(
			tr("Parental controls are currently managed by the "
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

	QByteArray hash = m_cfg->getParentalControlsLocked();
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
						m_cfg->setParentalControlsLocked(QByteArray());
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
					m_cfg->setParentalControlsLocked(
						server::passwordhash::hash(pass));
				}
			}
		});
	dlg->show();
}

}
}
