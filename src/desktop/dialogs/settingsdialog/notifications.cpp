// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/notifications.h"
#include "desktop/main.h"
#include "desktop/notifications.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/config/config.h"
#include <QCheckBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QToolButton>
#include <QVBoxLayout>
#ifdef Q_OS_ANDROID
#	include "libclient/utils/androidutils.h"
#endif

namespace dialogs {
namespace settingsdialog {

Notifications::Notifications(config::Config *cfg, QWidget *parent)
	: Page(parent)
{
	init(cfg);
}

void Notifications::setUp(config::Config *cfg, QVBoxLayout *layout)
{
#if defined(Q_OS_ANDROID) && defined(DRAWPILE_USE_CONNECT_SERVICE)
	initAndroid(cfg, utils::addFormSection(layout));
	utils::addFormSeparator(layout);
#endif
	initGrid(cfg, layout);
	utils::addFormSeparator(layout);
	initOptions(cfg, utils::addFormSection(layout));
}

#if defined(Q_OS_ANDROID) && defined(DRAWPILE_USE_CONNECT_SERVICE)
void Notifications::initAndroid(config::Config *cfg, QFormLayout *form)
{
	QCheckBox *connectionNotification =
		new QCheckBox(tr("Display notification while connected to a session"));
	form->addRow(tr("Network:"), connectionNotification);
	connectionNotification->setChecked(cfg->getConnectionNotification());
	connect(
		connectionNotification, &QCheckBox::clicked, this,
		[this, connectionNotification, cfg](bool checked) {
			if(checked) {
				connectionNotification->setEnabled(false);

				bool shoudlShowRationale =
					utils::shoulShowPostNotificationsRationale();
				qDebug("Should show rationale %d", int(shoudlShowRationale));

				QMessageBox *box = utils::showQuestion(
					this, tr("Notification"),
					tr("Do you want to grant Drawpile permission to show "
					   "you a connection notification?"));
				connect(
					box, &QMessageBox::finished, this,
					[connectionNotification, cfg](int result) {
						if(result == int(QMessageBox::Yes)) {
							if(!utils::createConnectionNotificationChannel()) {
								qWarning(
									"Failed to create connection notification "
									"channel");
							}
							cfg->setConnectionNotification(true);
							connectionNotification->setChecked(true);
						} else {
							connectionNotification->setChecked(false);
						}
						connectionNotification->setEnabled(true);
					});
			} else {
				cfg->setConnectionNotification(false);
			}
		});
}
#endif

void Notifications::initGrid(config::Config *cfg, QVBoxLayout *layout)
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

#define ADD_NOTIFICATION_ROW(TEXT, SETTING, EVENT)                             \
	do {                                                                       \
		QString text = tr(TEXT);                                               \
                                                                               \
		QLabel *label = new QLabel(this);                                      \
		label->setText(TEXT);                                                  \
		grid->addWidget(label, row, 0, Qt::AlignLeft);                         \
                                                                               \
		QCheckBox *soundBox = new QCheckBox(this);                             \
		soundBox->setToolTip(std::get<1>(header[1]));                          \
		CFG_BIND_CHECKBOX(cfg, NotifSound##SETTING, soundBox);                 \
		grid->addWidget(soundBox, row, 1, Qt::AlignHCenter);                   \
                                                                               \
		QCheckBox *popupBox = new QCheckBox(this);                             \
		popupBox->setToolTip(std::get<1>(header[2]));                          \
		CFG_BIND_CHECKBOX(cfg, NotifPopup##SETTING, popupBox);                 \
		grid->addWidget(popupBox, row, 2, Qt::AlignHCenter);                   \
                                                                               \
		QCheckBox *flashBox = new QCheckBox(this);                             \
		flashBox->setToolTip(std::get<1>(header[3]));                          \
		CFG_BIND_CHECKBOX(cfg, NotifFlash##SETTING, flashBox);                 \
		grid->addWidget(flashBox, row, 3, Qt::AlignHCenter);                   \
                                                                               \
		QToolButton *preview = new QToolButton(this);                          \
		preview->setText(tr("Preview event"));                                 \
		preview->setToolTip(std::get<1>(header[4]));                           \
		preview->setIcon(QIcon::fromTheme("media-playback-start"));            \
		grid->addWidget(preview, row, 4, Qt::AlignHCenter);                    \
                                                                               \
		connect(preview, &QToolButton::clicked, [this, text] {                 \
			dpApp().notifications()->preview(                                  \
				this, notification::Event::EVENT, text);                       \
		});                                                                    \
                                                                               \
		++row;                                                                 \
	} while(0)

	int row = 1;
	ADD_NOTIFICATION_ROW("Chat message", Chat, Chat);
	ADD_NOTIFICATION_ROW("Private message", PrivateChat, PrivateChat);
	ADD_NOTIFICATION_ROW("User joined", Login, Login);
	ADD_NOTIFICATION_ROW("User left", Logout, Logout);
	ADD_NOTIFICATION_ROW("Canvas locked", Lock, Locked);
	ADD_NOTIFICATION_ROW("Canvas unlocked", Unlock, Unlocked);
	ADD_NOTIFICATION_ROW("Disconnected", Disconnect, Disconnect);

#undef ADD_NOTIFICATION_ROW

	layout->addLayout(grid);
}

void Notifications::initOptions(config::Config *cfg, QFormLayout *form)
{
	KisSliderSpinBox *volume = new KisSliderSpinBox(this);
	volume->setMaximum(100);
	volume->setSuffix(tr("%"));
	CFG_BIND_SLIDERSPINBOX(cfg, SoundVolume, volume);
	form->addRow(tr("Sound volume:"), volume);
	disableKineticScrollingOnWidget(volume);

	QCheckBox *mentionEnabled =
		new QCheckBox(tr("Use private message notification for mentions"));
	CFG_BIND_CHECKBOX(cfg, MentionEnabled, mentionEnabled);
	form->addRow(tr("Mentions:"), mentionEnabled);

	QPlainTextEdit *triggerList = new QPlainTextEdit;
	triggerList->setFixedHeight(100);
	triggerList->setPlaceholderText(
		tr("Additional triggers go here, one per line."));
	CFG_BIND_PLAINTEXTEDIT(cfg, MentionTriggerList, triggerList);
	CFG_BIND_SET(cfg, MentionEnabled, triggerList, QWidget::setEnabled);
	form->addRow(nullptr, triggerList);
	form->addRow(
		nullptr, utils::formNote(
					 tr("Your username always counts as a mention. You can "
						"add additional trigger words or phrases that you "
						"want to count as well, such as other nicknames. One "
						"word or phrase per line, case doesn't matter.")));
}

}
}
