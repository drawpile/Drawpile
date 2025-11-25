// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog/network.h"
#include "desktop/dialogs/avatarimport.h"
#include "desktop/dialogs/settingsdialog/helpers.h"
#include "desktop/main.h"
#include "desktop/utils/accountlistmodel.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/config/config.h"
#include "libclient/utils/avatarlistmodel.h"
#include "libclient/utils/avatarlistmodeldelegate.h"
#include "libshared/net/messagequeue.h"
#include "libshared/net/proxy.h"
#include <QButtonGroup>
#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QMessageBox>
#include <QModelIndex>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>
#ifdef Q_OS_ANDROID
#	include "libshared/util/androidutils.h"
#endif

namespace dialogs {
namespace settingsdialog {

Network::Network(config::Config *cfg, QWidget *parent)
	: Page(parent)
{
	init(cfg);
}

void Network::setUp(config::Config *cfg, QVBoxLayout *layout)
{
	initAvatars(layout);
	utils::addFormSeparator(layout);
	initNetwork(cfg, utils::addFormSection(layout));
#ifdef DP_HAVE_BUILTIN_SERVER
	utils::addFormSeparator(layout);
	initBuiltinServer(cfg, utils::addFormSection(layout));
#endif
}

void Network::initAvatars(QVBoxLayout *layout)
{
	QLabel *avatarsLabel = new QLabel(tr("Chat avatars:"), this);
	layout->addWidget(avatarsLabel);

	QListView *avatars = new QListView(this);
	avatarsLabel->setBuddy(avatars);
	avatars->setViewMode(QListView::IconMode);
	avatars->setResizeMode(QListView::Adjust);
	avatars->setMovement(QListView::Static);
	avatars->setUniformItemSizes(true);
	avatars->setWrapping(true);
	avatars->setMinimumHeight(40);
	avatars->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
	avatars->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
	avatars->setSelectionMode(QAbstractItemView::ExtendedSelection);
	utils::bindKineticScrollingWith(
		avatars, Qt::ScrollBarAlwaysOff, Qt::ScrollBarAsNeeded);

	AvatarListModel *avatarsModel = new AvatarListModel(true, this);
	avatarsModel->loadAvatars();
	avatars->setModel(avatarsModel);
	avatars->setItemDelegate(new AvatarItemDelegate(this));

	layout->addWidget(avatars);
	layout->addLayout(listActions(
		avatars, tr("Add"), tr("Add avatar…"),
		[=] {
			AvatarImport::importAvatar(avatarsModel, this);
		},
		tr("Remove"), tr("Delete selected avatars…"),
		makeDefaultDeleter(
			this, avatars, tr("Delete avatars"),
			QT_TR_N_NOOP("Really delete %n avatar(s)?"))));
}

void Network::initBuiltinServer(config::Config *cfg, QFormLayout *form)
{
	QSpinBox *port = new QSpinBox;
	port->setAlignment(Qt::AlignLeft);
	port->setRange(1, UINT16_MAX);
	CFG_BIND_SPINBOX(cfg, ServerPort, port);
	form->addRow(
		tr("Builtin server:"),
		utils::encapsulate(tr("Host on port %1 if available"), port));
}

void Network::initNetwork(config::Config *cfg, QFormLayout *form)
{
	QCheckBox *checkForUpdates =
		new QCheckBox(tr("Automatically check for updates"), this);
	CFG_BIND_CHECKBOX(cfg, UpdateCheckEnabled, checkForUpdates);
	form->addRow(tr("Updates:"), checkForUpdates);

	QCheckBox *allowInsecure =
		new QCheckBox(tr("Allow insecure local storage"), this);
	CFG_BIND_CHECKBOX(cfg, InsecurePasswordStorage, allowInsecure);
	connect(allowInsecure, &QCheckBox::clicked, this, [](bool checked) {
		if(!checked) {
			AccountListModel(dpApp().state(), nullptr).clearFallbackPasswords();
		}
	});
	form->addRow(tr("Password security:"), allowInsecure);

	utils::FormNote *allowInsecureNotice = utils::formNote(
		tr("With this enabled, Drawpile may save passwords in an unencrypted "
		   "format. Disabling it will forget any insecurely stored passwords."),
		QSizePolicy::Label, QIcon::fromTheme("dialog-warning"));
	form->addRow(nullptr, allowInsecureNotice);
	CFG_BIND_SET(
		cfg, InsecurePasswordStorage, allowInsecureNotice, QWidget::setVisible);

	QButtonGroup *autoReset = utils::addRadioGroup(
		form, tr("Connection quality:"), true,
		{{tr("Good"), 1}, {tr("Poor"), 0}});
	CFG_BIND_BUTTONGROUP(cfg, ServerAutoReset, autoReset);

	QSpinBox *timeout = new QSpinBox(this);
	timeout->setAlignment(Qt::AlignLeft);
	timeout->setRange(15, 600);
	CFG_BIND_SPINBOX(cfg, ServerTimeout, timeout);
	form->addRow(
		tr("Network timeout:"), utils::encapsulate(tr("%1 seconds"), timeout));

#ifndef __EMSCRIPTEN__
	QButtonGroup *proxy = utils::addRadioGroup(
		form, tr("Network proxy:"), true,
		{{tr("System"), int(net::ProxyMode::Default)},
		 {tr("Disabled"), int(net::ProxyMode::Disabled)}});
	CFG_BIND_BUTTONGROUP(cfg, NetworkProxyMode, proxy);
#endif

	KisSliderSpinBox *messageQueueDrainRate = new KisSliderSpinBox;
	messageQueueDrainRate->setRange(
		0, net::MessageQueue::MAX_SMOOTH_DRAIN_RATE);
	CFG_BIND_SLIDERSPINBOX(cfg, MessageQueueDrainRate, messageQueueDrainRate);
	form->addRow(tr("Receive delay:"), messageQueueDrainRate);
	form->addRow(
		nullptr, utils::formNote(
					 tr("The higher the value, the smoother "
						"strokes from other users come in.")));
	disableKineticScrollingOnWidget(messageQueueDrainRate);
}

}
}
