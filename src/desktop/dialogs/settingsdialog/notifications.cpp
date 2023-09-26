// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/notifications.h"
#include "desktop/dialogs/settingsdialog/helpers.h"
#include "desktop/notifications.h"
#include "desktop/settings.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QToolButton>
#include <QVBoxLayout>

namespace dialogs {
namespace settingsdialog {

Notifications::Notifications(
	desktop::settings::Settings &settings, QWidget *parent)
	: Page(parent)
{
	init(settings);
}

void Notifications::setUp(
	desktop::settings::Settings &settings, QVBoxLayout *layout)
{
	initSounds(settings, utils::addFormSection(layout));
}

void Notifications::initSounds(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	auto *volume = new KisSliderSpinBox(this);
	volume->setMaximum(100);
	volume->setSuffix(tr("%"));
	settings.bindSoundVolume(volume);
	form->addRow(tr("Sound volume:"), volume);

	using Settings = desktop::settings::Settings;
	const std::initializer_list<std::tuple<
		QString,
		std::pair<QMetaObject::Connection, QMetaObject::Connection> (
			desktop::settings::Settings::*)(QCheckBox *&&args),
		notification::Event>>
		sounds = {
			{tr("Message received"),
			 &Settings::bindNotificationChat<QCheckBox *>,
			 notification::Event::CHAT},
			{tr("Recording marker"),
			 &Settings::bindNotificationMarker<QCheckBox *>,
			 notification::Event::MARKER},
			{tr("User joined"), &Settings::bindNotificationLogin<QCheckBox *>,
			 notification::Event::LOGIN},
			{tr("User parted"), &Settings::bindNotificationLogout<QCheckBox *>,
			 notification::Event::LOGOUT},
			{tr("Canvas locked"), &Settings::bindNotificationLock<QCheckBox *>,
			 notification::Event::LOCKED},
			{tr("Canvas unlocked"),
			 &Settings::bindNotificationUnlock<QCheckBox *>,
			 notification::Event::UNLOCKED},
		};

	QGridLayout *grid = new QGridLayout;
	grid->setColumnStretch(0, 1);
	int row = 0;
	for(auto [text, bindable, event] : sounds) {
		auto *checkbox = new QCheckBox(text, this);
		((&settings)->*bindable)(std::forward<QCheckBox *>(checkbox));
		grid->addWidget(checkbox, row, 0);

		auto *preview = new QToolButton(this);
		preview->setFixedHeight(checkbox->sizeHint().height());
		preview->setText(tr("Preview sound"));
		preview->setIcon(QIcon::fromTheme("audio-volume-high"));
		grid->addWidget(preview, row, 1);

		connect(preview, &QToolButton::clicked, [event = event, &settings] {
			notification::playSoundNow(event, settings.soundVolume());
		});
		settings.bindSoundVolume(checkbox, &QCheckBox::setEnabled);
		settings.bindSoundVolume(preview, &QToolButton::setEnabled);

		++row;
	}
	form->addRow(tr("Notifications:"), grid);
}

} // namespace settingsdialog
} // namespace dialogs
