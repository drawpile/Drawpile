// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/settingsdialog/contentfilter.h"
#include "desktop/dialogs/settingsdialog/helpers.h"
#include "desktop/settings.h"
#include "desktop/utils/sanerformlayout.h"
#include "libclient/contentfilter/contentfilter.h"
#include "libshared/util/passwordhash.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>

namespace dialogs {
namespace settingsdialog {

ContentFilter::ContentFilter(desktop::settings::Settings &settings, QWidget *parent)
	: QWidget(parent)
	, m_settings(settings)
{
	auto *layout = new QVBoxLayout(this);

	initInfoBar(layout);

	if (contentfilter::isOSActive()) {
		initOsManaged(layout);
	} else {
		initBuiltIn(settings, layout);
	}
}

void ContentFilter::initBuiltIn(desktop::settings::Settings &settings, QVBoxLayout *layout)
{
	auto *stack = new QStackedWidget;
	stack->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(stack);

	auto *formPage = new QWidget;
	auto *form = new utils::SanerFormLayout(formPage);
	form->setContentsMargins(0, 0, 0, 0);
	form->setStretchLabel(false);
	stack->addWidget(formPage);

	auto *advisory = new QCheckBox(tr("Filter sessions with a content advisory"));
	settings.bindContentFilterAdvisoryTag(advisory);
	form->addRow("Advisory filtering:", advisory);
	form->addRow(nullptr, utils::note(tr("Advisories are added by session owners "
		"when they believe a session may not be safe for everyone."), QSizePolicy::CheckBox));

	form->addSpacer();

	auto *autoTag = new QCheckBox(tr("Filter sessions with titles containing:"));
	form->addRow(tr("Keyword filtering:"), autoTag);
	settings.bindContentFilterAutoTag(autoTag);
	auto *tagWords = new QLineEdit(this);
	tagWords->setPlaceholderText(contentfilter::defaultWordList());
	utils::setSpacingControlType(tagWords, QSizePolicy::CheckBox);
	form->addRow(nullptr, utils::indent(tagWords));
	settings.bindContentFilterTags(tagWords);
	settings.bindContentFilterAutoTag(tagWords, &QLineEdit::setEnabled);

	form->addSpacer();

	auto *level = form->addRadioGroup(tr("Filter mode:"), false, {
		{ tr("Unrestricted"), int(contentfilter::Level::Unrestricted) },
		{ tr("Remove sessions from listings"), int(contentfilter::Level::NoList) },
		{ tr("Prevent joining filtered sessions"), int(contentfilter::Level::NoJoin) },
		{ tr("Disconnect when sessions become not safe for me"), int(contentfilter::Level::Restricted) }
	});
	level->button(1)->setToolTip(tr("Sessions will be hidden from server "
		"listings, but can still be joined by following a direct link."));
	level->button(2)->setToolTip(tr("Sessions will be hidden from server "
		"listings and cannot be joined."));
	level->button(3)->setToolTip(tr("Sessions will be hidden from server "
		"listings and cannot be joined. Connected sessions that change "
		"their title or advisory flags will automatically disconnect."));
	settings.bindContentFilterLevel(level);

	auto *forceCensor = new QCheckBox(tr("Block layer uncensoring"));
	settings.bindContentFilterForceCensor(forceCensor);
	form->addRow(nullptr, forceCensor);

	layout->addStretch();

	auto *lockLayout = new QHBoxLayout;
	lockLayout->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(lockLayout);

	auto *lock = new QPushButton(tr("Lock"));
	lock->setAutoDefault(false);
	lockLayout->addWidget(lock);
	connect(lock, &QPushButton::clicked, this, &ContentFilter::toggleLock);

	auto *hide = new QCheckBox(tr("Hide settings when locked"));
	lockLayout->addWidget(hide);
	settings.bindContentFilterHideLocked(hide);

	lockLayout->addStretch();

	auto *hiddenPage = new QWidget;
	auto *hiddenLayout = new QVBoxLayout(hiddenPage);
	hiddenLayout->setContentsMargins(0, 0, 0, 0);
	hiddenLayout->addWidget(new QLabel(tr("Content filtering settings are currently locked.")), 0, Qt::AlignCenter);
	stack->addWidget(hiddenPage);

	settings.bindContentFilterLocked(this, [=, &settings](QByteArray hash) {
		const auto locked = !hash.isEmpty();
		advisory->setDisabled(locked);
		tagWords->setEchoMode(locked ? QLineEdit::Password : QLineEdit::Normal);
		tagWords->setDisabled(locked || !settings.contentFilterAutoTag());
		autoTag->setDisabled(locked);
		for (auto *button : level->buttons()) {
			button->setDisabled(locked);
		}
		forceCensor->setDisabled(locked);
		hide->setDisabled(locked);
		lock->setText(locked ? tr("Unlock") : tr("Lock"));

		stack->setCurrentWidget(locked && settings.contentFilterHideLocked()
			? hiddenPage
			: formPage
		);
	});
}

void ContentFilter::initInfoBar(QVBoxLayout *layout)
{
	auto *info = new QHBoxLayout;
	info->setContentsMargins(0, 0, 0, 0);
	layout->addLayout(info);

	info->addWidget(makeIconLabel(QStyle::SP_MessageBoxInformation, QStyle::PM_LargeIconSize, this));

	auto *description = new QLabel;
	description->setWordWrap(true);
	description->setText(tr("Content filters allow you to personalize your online Drawpile experience by hiding content that’s not safe for you (NSFM)."));
	info->addWidget(description);

	layout->addWidget(utils::makeSeparator());
}

void ContentFilter::initOsManaged(QVBoxLayout *layout)
{
	auto *osManagedLayout = new QVBoxLayout;
	osManagedLayout->setContentsMargins(0, 0, 0, 0);
	osManagedLayout->addWidget(new QLabel(tr("Content filtering settings are currently managed by the operating system.")), 0, Qt::AlignCenter);
	layout->addLayout(osManagedLayout, 1);
}

void ContentFilter::toggleLock()
{
	const auto hash = m_settings.contentFilterLocked();
	const auto locked = !hash.isEmpty();
	const auto title = locked
		? tr("Unlock Content Filter Settings")
		: tr("Lock Content Filter Settings");

	for (;;) {
		auto ok = true;
		const auto pass = QInputDialog::getText(
			this, title, tr("Enter password:"), QLineEdit::Password,
			QString(), &ok, Qt::Sheet
		);
		if (!ok) {
			break;
		} else if (locked) {
			if (server::passwordhash::check(pass, hash)) {
				m_settings.setContentFilterLocked(QByteArray());
				break;
			} else {
				QMessageBox box(
					QMessageBox::Warning,
					title,
					tr("Incorrect password."),
					QMessageBox::Retry | QMessageBox::Cancel,
					this
				);
				box.setWindowModality(Qt::WindowModal);
				if (box.exec() == QMessageBox::Cancel) {
					break;
				}
			}
		} else {
			m_settings.setContentFilterLocked(server::passwordhash::hash(pass));
			break;
		}
	}
}

} // namespace settingsdialog
} // namespace dialogs
