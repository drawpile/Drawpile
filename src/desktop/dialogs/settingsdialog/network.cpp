// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/avatarimport.h"
#include "desktop/dialogs/settingsdialog/helpers.h"
#include "desktop/dialogs/settingsdialog/network.h"
#include "desktop/settings.h"
#include "desktop/utils/sanerformlayout.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_slider_spin_box.h"
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

namespace dialogs {
namespace settingsdialog {

Network::Network(desktop::settings::Settings &settings, QWidget *parent)
	: QWidget(parent)
{
	// TODO: make the form layout work with RTL or go back to a QFormLayout.
	setLayoutDirection(Qt::LeftToRight);
	auto *form = new utils::SanerFormLayout(this);
	form->setStretchLabel(false);

	initAvatars(form);
	form->addSeparator();

	auto *checkForUpdates = new QCheckBox(tr("Automatically check for updates"), this);
	settings.bindUpdateCheckEnabled(checkForUpdates);
	form->addRow(tr("Updates:"), checkForUpdates);

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
	autoResetNoteLayout->insertWidget(0, utils::makeIconLabel(QIcon::fromTheme("dialog-warning"), QStyle::PM_SmallIconSize, this), 0, Qt::AlignTop);
	autoResetNote->setLayout(autoResetNoteLayout);
	form->addRow(nullptr, autoResetNote);
	settings.bindServerAutoReset(autoResetNote, &QWidget::setHidden);

	auto *timeout = new QSpinBox(this);
	timeout->setAlignment(Qt::AlignLeft);
	timeout->setRange(15, 600);
	settings.bindServerTimeout(timeout);
	form->addRow(tr("Network timeout:"), utils::encapsulate(tr("%1 seconds"), timeout));

	auto *messageQueueDrainRate = new KisSliderSpinBox;
	messageQueueDrainRate->setRange(0, libclient::settings::maxMessageQueueDrainRate);
	settings.bindMessageQueueDrainRate(messageQueueDrainRate);
	form->addRow(tr("Receive delay:"), messageQueueDrainRate);
	form->addRow(nullptr, utils::note(tr("The higher the value, the smoother strokes from other users come in."), QSizePolicy::Label));
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
	utils::initKineticScrolling(avatars);

	auto *avatarsModel = new AvatarListModel(true, this);
	avatarsModel->loadAvatars();
	avatars->setModel(avatarsModel);
	avatars->setItemDelegate(new AvatarItemDelegate(this));

	form->addSpanningRow(avatars);
	form->addSpanningRow(listActions(avatars, tr("Add avatar…"), [=] {
		AvatarImport::importAvatar(avatarsModel, this);
	}, tr("Delete selected avatars…"), makeDefaultDeleter(this, avatars, tr("Delete avatars"), QT_TR_N_NOOP("Really delete %n avatar(s)?"))));
}

} // namespace settingsdialog
} // namespace dialogs
