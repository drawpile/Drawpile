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
#include <QPlainTextEdit>
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
		{tr("Private message"),
		 &Settings::bindNotifSoundPrivateChat<QCheckBox *>,
		 &Settings::bindNotifPopupPrivateChat<QCheckBox *>,
		 &Settings::bindNotifFlashPrivateChat<QCheckBox *>,
		 notification::Event::PrivateChat},
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
		// See soundplayer_win32.cpp for an explanation.
#if defined(Q_OS_WIN) && !defined(_WIN64)
		soundBox->setEnabled(false);
#else
		((&settings)->*bindSound)(std::forward<QCheckBox *>(soundBox));
#endif
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
		preview->setToolTip(std::get<1>(header[4]));
		preview->setIcon(QIcon::fromTheme("media-playback-start"));
		grid->addWidget(preview, row, 4, Qt::AlignHCenter);

		connect(
			preview, &QToolButton::clicked,
			[this, previewText = text, previewEvent = event] {
				dpApp().notifications()->preview(
					this, previewEvent, previewText);
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
	// See soundplayer_win32.cpp for an explanation.
#if defined(Q_OS_WIN) && !defined(_WIN64)
	volume->setValue(0);
	volume->setEnabled(false);
#else
	settings.bindSoundVolume(volume);
#endif
	form->addRow(tr("Sound volume:"), volume);
	disableKineticScrollingOnWidget(volume);

	QCheckBox *mentionEnabled =
		new QCheckBox(tr("Use private message notification for mentions"));
	settings.bindMentionEnabled(mentionEnabled);
	form->addRow(tr("Mentions:"), mentionEnabled);

	QPlainTextEdit *triggerList = new QPlainTextEdit;
	triggerList->setFixedHeight(100);
	triggerList->setPlaceholderText(
		tr("Additional triggers go here, one per line."));
	settings.bindMentionTriggerList(triggerList);
	settings.bindMentionEnabled(triggerList, &QWidget::setEnabled);
	form->addRow(nullptr, triggerList);
	form->addRow(
		nullptr, utils::formNote(
					 tr("Your username always counts as a mention. You can "
						"add additional trigger words or phrases that you "
						"want to count as well, such as other nicknames. One "
						"word or phrase per line, case doesn't matter.")));
}

} // namespace settingsdialog
} // namespace dialogs
