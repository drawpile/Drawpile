// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/notifications.h"
#include "desktop/dialogs/settingsdialog/helpers.h"
#include "desktop/main.h"
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
	initGrid(settings, layout);
	utils::addFormSeparator(layout);
	initOptions(settings, utils::addFormSection(layout));
}

void Notifications::initGrid(
	desktop::settings::Settings &settings, QVBoxLayout *layout)
{
	QGridLayout *grid = new QGridLayout;

	std::tuple<QString, QString, Qt::Alignment> header[] = {
		{tr("Event"), tr("What triggered this notification"), Qt::AlignLeft},
		{tr("Play sound"), tr("Play a sound effect when this event occurs."),
		 Qt::AlignHCenter},
		{tr("Popup"),
		 tr("Pop up a speech bubble in the corner when this event occurs."),
		 Qt::AlignHCenter},
#if defined(Q_OS_WIN)
		{tr("Flash taskbar"),
		 tr("Flash the Drawpile window in the taskbar when this event occurs."),
		 Qt::AlignHCenter},
#elif defined(Q_OS_MACOS)
		{tr("Bounce dock"),
		 tr("Bounce Drawpile's icon in the dock when this event occurs."),
		 Qt::AlignHCenter},
#else
		{tr("Demand attention"),
		 tr("Mark Drawpile's window as requiring attention. How exactly this "
			"looks depends on your system."),
		 Qt::AlignHCenter},
#endif
		{tr("Preview"), tr("Trigger this event so you can see what happens."),
		 Qt::AlignHCenter},
	};
	int column = 0;
	for(auto [text, tooltip, alignment] : header) {
		QLabel *label = new QLabel(this);
		label->setText(QStringLiteral("<b>%1</b>").arg(text));
		label->setToolTip(tooltip);
		grid->addWidget(label, 0, column++, alignment);
	}

	using Settings = desktop::settings::Settings;
	using Bind = std::pair<QMetaObject::Connection, QMetaObject::Connection> (
		desktop::settings::Settings::*)(QCheckBox *&&args);
	std::tuple<QString, Bind, Bind, Bind, notification::Event> sounds[] = {
		{tr("Chat message"), &Settings::bindNotifSoundChat<QCheckBox *>,
		 &Settings::bindNotifPopupChat<QCheckBox *>,
		 &Settings::bindNotifFlashChat<QCheckBox *>, notification::Event::Chat},
		{tr("User joined"), &Settings::bindNotifSoundLogin<QCheckBox *>,
		 &Settings::bindNotifPopupLogin<QCheckBox *>,
		 &Settings::bindNotifFlashLogin<QCheckBox *>,
		 notification::Event::Login},
		{tr("User left"), &Settings::bindNotifSoundLogout<QCheckBox *>,
		 &Settings::bindNotifPopupLogout<QCheckBox *>,
		 &Settings::bindNotifFlashLogout<QCheckBox *>,
		 notification::Event::Logout},
		{tr("Canvas locked"), &Settings::bindNotifSoundLock<QCheckBox *>,
		 &Settings::bindNotifPopupLock<QCheckBox *>,
		 &Settings::bindNotifFlashLock<QCheckBox *>,
		 notification::Event::Locked},
		{tr("Canvas unlocked"), &Settings::bindNotifSoundUnlock<QCheckBox *>,
		 &Settings::bindNotifPopupUnlock<QCheckBox *>,
		 &Settings::bindNotifFlashUnlock<QCheckBox *>,
		 notification::Event::Unlocked},
		{tr("Disconnected"), &Settings::bindNotifSoundDisconnect<QCheckBox *>,
		 &Settings::bindNotifPopupDisconnect<QCheckBox *>,
		 &Settings::bindNotifFlashDisconnect<QCheckBox *>,
		 notification::Event::Disconnect},
	};
	int row = 1;
	for(auto [text, bindSound, bindPopup, bindFlash, event] : sounds) {
		QLabel *label = new QLabel(this);
		label->setText(text);
		grid->addWidget(label, row, 0, Qt::AlignLeft);

		QCheckBox *soundBox = new QCheckBox(this);
		soundBox->setToolTip(std::get<1>(header[1]));
		((&settings)->*bindSound)(std::forward<QCheckBox *>(soundBox));
		grid->addWidget(soundBox, row, 1, Qt::AlignHCenter);

		QCheckBox *popupBox = new QCheckBox(this);
		popupBox->setToolTip(std::get<1>(header[2]));
		((&settings)->*bindPopup)(std::forward<QCheckBox *>(popupBox));
		grid->addWidget(popupBox, row, 2, Qt::AlignHCenter);

		QCheckBox *flashBox = new QCheckBox(this);
		flashBox->setToolTip(std::get<1>(header[3]));
		((&settings)->*bindFlash)(std::forward<QCheckBox *>(flashBox));
		grid->addWidget(flashBox, row, 3, Qt::AlignHCenter);

		QToolButton *preview = new QToolButton(this);
		preview->setText(tr("Preview event"));
		flashBox->setToolTip(std::get<1>(header[4]));
		preview->setIcon(QIcon::fromTheme("media-playback-start"));
		grid->addWidget(preview, row, 4, Qt::AlignHCenter);

		connect(
			preview, &QToolButton::clicked, [this, text = text, event = event] {
				dpApp().notifications()->trigger(this, event, text, true);
			});

		++row;
	}
	layout->addLayout(grid);
}

void Notifications::initOptions(
	desktop::settings::Settings &settings, QFormLayout *form)
{
	KisSliderSpinBox *volume = new KisSliderSpinBox(this);
	volume->setMaximum(100);
	volume->setSuffix(tr("%"));
	settings.bindSoundVolume(volume);
	form->addRow(tr("Sound volume:"), volume);
}

} // namespace settingsdialog
} // namespace dialogs
