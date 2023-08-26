// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/settingsdialog/notifications.h"
#include "desktop/dialogs/settingsdialog/helpers.h"
#include "desktop/notifications.h"
#include "desktop/settings.h"
#include "desktop/utils/sanerformlayout.h"
#include "desktop/widgets/kis_slider_spin_box.h"

#include <QCheckBox>
#include <QToolButton>

namespace dialogs {
namespace settingsdialog {

Notifications::Notifications(
	desktop::settings::Settings &settings, QWidget *parent)
	: QWidget(parent)
{
	// TODO: make the form layout work with RTL or go back to a QFormLayout.
	setLayoutDirection(Qt::LeftToRight);
	auto *form = new utils::SanerFormLayout(this);
	form->setStretchLabel(false);
	initSounds(settings, form);
}

void Notifications::initSounds(
	desktop::settings::Settings &settings, utils::SanerFormLayout *form)
{
	auto *volume = new KisSliderSpinBox(this);
	volume->setMaximum(100);
	volume->setSuffix(tr("%"));
	settings.bindSoundVolume(volume);
	form->addRow(tr("Sound volume:"), volume, 1, 2);

	using Settings = desktop::settings::Settings;
	const std::initializer_list<std::tuple<
		QString,
		std::pair<QMetaObject::Connection, QMetaObject::Connection> (
			desktop::settings::Settings::*)(QCheckBox * &&args),
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

	auto first = true;
	for(auto [text, bindable, event] : sounds) {
		auto *checkbox = new QCheckBox(text, this);
		((&settings)->*bindable)(std::forward<QCheckBox *>(checkbox));
		const auto row = form->rowCount();
		form->addRow(first ? tr("Notifications:") : QString(), checkbox);
		auto *preview = new QToolButton(this);
		preview->setFixedHeight(checkbox->sizeHint().height());
		preview->setText(tr("Preview sound"));
		preview->setIcon(QIcon::fromTheme("audio-volume-high"));
		connect(preview, &QToolButton::clicked, [event = event, &settings] {
			notification::playSoundNow(event, settings.soundVolume());
		});
		form->addAside(preview, row);
		first = false;
		settings.bindSoundVolume(checkbox, &QCheckBox::setEnabled);
		settings.bindSoundVolume(preview, &QToolButton::setEnabled);
	}
}

} // namespace settingsdialog
} // namespace dialogs
