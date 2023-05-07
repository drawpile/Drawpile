// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/avatarimport.h"
#include "desktop/dialogs/settingsdialog/helpers.h"
#include "desktop/dialogs/settingsdialog/network.h"
#include "desktop/notifications.h"
#include "desktop/settings.h"
#include "desktop/utils/sanerformlayout.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "libclient/utils/avatarlistmodel.h"
#include "libclient/utils/avatarlistmodeldelegate.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMessageBox>
#include <QModelIndex>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <algorithm>

namespace dialogs {
namespace settingsdialog {

Network::Network(desktop::settings::Settings &settings, QWidget *parent)
	: QWidget(parent)
{
	auto *form = new utils::SanerFormLayout(this);
	form->setStretchLabel(false);

	initAvatars(form);
	form->addSeparator();
	initSounds(settings, form);
	form->addSeparator();

	auto *allowInsecure = new QCheckBox(tr("Allow insecure local storage"), this);
	settings.bindInsecurePasswordStorage(allowInsecure);
	form->addRow(tr("Password security:"), allowInsecure);

	auto *autoReset = form->addRadioGroup(tr("Connection quality:"), true, {
		{ tr("Good"), 1 },
		{ tr("Poor"), 0 }
	});
	settings.bindServerAutoReset(autoReset);
	auto *autoResetNote = new QWidget;
	auto *autoResetNoteLayout = utils::note(tr("If all operators in a session set connection quality to Poor, auto-reset will not work and the server will stop processing updates until the session is manually reset."), QSizePolicy::Label);
	autoResetNoteLayout->insertWidget(0, makeIconLabel(QStyle::SP_MessageBoxWarning, QStyle::PM_SmallIconSize, this), 0, Qt::AlignTop);
	autoResetNote->setLayout(autoResetNoteLayout);
	form->addRow(nullptr, autoResetNote);
	settings.bindServerAutoReset(autoResetNote, &QWidget::setHidden);

	auto *timeout = new QSpinBox(this);
	timeout->setAlignment(Qt::AlignLeft);
	timeout->setRange(15, 600);
	settings.bindServerTimeout(timeout);
	form->addRow(tr("Network timeout:"), utils::encapsulate(tr("%1 seconds"), timeout));
}

void Network::initAvatars(utils::SanerFormLayout *form)
{
	auto *avatarsLabel = new QLabel(tr("Chat avatars:"), this);
	form->addSpanningRow(avatarsLabel);

	auto *avatars = new QListView(this);
	avatarsLabel->setBuddy(avatars);
	avatars->setViewMode(QListView::IconMode);
	avatars->setResizeMode(QListView::Adjust);
	avatars->setMovement(QListView::Static);
	avatars->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	avatars->setUniformItemSizes(true);
	avatars->setWrapping(true);
	avatars->setMinimumHeight(40);
	avatars->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
	avatars->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
	avatars->setSelectionMode(QAbstractItemView::ExtendedSelection);

	auto *avatarsModel = new AvatarListModel(this);
	avatarsModel->loadAvatars();
	avatars->setModel(avatarsModel);
	avatars->setItemDelegate(new AvatarItemDelegate(this));

	form->addSpanningRow(avatars);
	form->addSpanningRow(listActions(avatars, tr("Add avatar…"), [=] {
		AvatarImport::importAvatar(avatarsModel, this);
	}, tr("Delete selected avatars…"), makeDefaultDeleter(this, avatars, tr("Delete avatars"), QT_TR_NOOP("Really delete %n avatars?"))));
}

void Network::initSounds(desktop::settings::Settings &settings, utils::SanerFormLayout *form)
{
	auto *volume = new QSlider(Qt::Horizontal, this);
	volume->setMaximum(100);
	volume->setTickPosition(QSlider::TicksBelow);
	volume->setTickInterval(100);
	settings.bindSoundVolume(volume);
	form->addRow(tr("Sound volume:"), utils::labelEdges(volume, tr("Off"), tr("Loud")), 1, 2);

	using Settings = desktop::settings::Settings;
	const std::initializer_list<
		std::tuple<
			QString,
			std::pair<QMetaObject::Connection, QMetaObject::Connection> (desktop::settings::Settings::*)(QCheckBox *&&args),
			notification::Event>
		> sounds = {
		{ tr("Message received"), &Settings::bindNotificationChat<QCheckBox *>,
			notification::Event::CHAT },
		{ tr("Recording marker"), &Settings::bindNotificationMarker<QCheckBox *>,
			notification::Event::MARKER },
		{ tr("User joined"), &Settings::bindNotificationLogin<QCheckBox *>,
			notification::Event::LOGIN },
		{ tr("User parted"), &Settings::bindNotificationLogout<QCheckBox *>,
			notification::Event::LOGOUT },
		{ tr("Canvas locked"), &Settings::bindNotificationLock<QCheckBox *>,
			notification::Event::LOCKED },
		{ tr("Canvas unlocked"), &Settings::bindNotificationUnlock<QCheckBox *>,
			notification::Event::UNLOCKED },
	};

	auto first = true;
	for (auto [ text, bindable, event ] : sounds) {
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
