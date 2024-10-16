// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog.h"
#include "desktop/dialogs/settingsdialog/general.h"
#include "desktop/dialogs/settingsdialog/network.h"
#include "desktop/dialogs/settingsdialog/notifications.h"
#include "desktop/dialogs/settingsdialog/parentalcontrols.h"
#include "desktop/dialogs/settingsdialog/servers.h"
#include "desktop/dialogs/settingsdialog/shortcuts.h"
#include "desktop/dialogs/settingsdialog/tablet.h"
#include "desktop/dialogs/settingsdialog/tools.h"
#include "desktop/dialogs/settingsdialog/touch.h"
#include "desktop/dialogs/settingsdialog/userinterface.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/utils/widgetutils.h"
#include <QAction>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace dialogs {

SettingsDialog::SettingsDialog(
	bool singleSession, bool smallScreenMode, QWidget *parent)
	: QDialog(parent)
	, m_settings(dpApp().settings())
{
	setWindowTitle(tr("Preferences"));
	resize(800, 600);

	setWindowModality(Qt::ApplicationModal);

#ifdef Q_OS_MACOS
	Q_UNUSED(smallScreenMode);
	bool vertical = false;
	bool menuFirst = true;
#elif defined(Q_OS_ANDROID)
	Q_UNUSED(smallScreenMode);
	bool vertical = false;
	bool menuFirst = false;
#else
	bool vertical = !smallScreenMode;
	bool menuFirst = !smallScreenMode;
#endif

	QWidget *menu = new QWidget;
	menu->setSizePolicy(
		vertical ? QSizePolicy::Minimum : QSizePolicy::MinimumExpanding,
		vertical ? QSizePolicy::MinimumExpanding : QSizePolicy::Minimum);
	menu->setBackgroundRole(QPalette::Midlight);
	menu->setAutoFillBackground(true);

	QBoxLayout *menuLayout = new QBoxLayout(
		vertical ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
	menu->setLayout(menuLayout);
	int menuMargin = style()->pixelMetric(QStyle::PM_ToolBarFrameWidth) +
					 style()->pixelMetric(QStyle::PM_ToolBarItemMargin);
	menuLayout->setContentsMargins(
		menuMargin, menuMargin, menuMargin, menuMargin);
	menuLayout->setSpacing(style()->pixelMetric(QStyle::PM_ToolBarItemSpacing));

	QScrollArea *menuScroll = new QScrollArea;
	utils::bindKineticScrollingWith(
		menuScroll, vertical ? Qt::ScrollBarAlwaysOff : Qt::ScrollBarAsNeeded,
		vertical ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
	menuScroll->setContentsMargins(0, 0, 0, 0);
	menuScroll->setWidgetResizable(true);
	menuScroll->setWidget(menu);

	// The servers page has nothing to configure if we're in single-session mode
	// in the browser, since the list server settings won't be available and
	// there's no trusted hosts configuration because the browser deals with it.
#ifdef __EMSCRIPTEN__
	bool showServers = !singleSession;
#else
	bool showServers = true;
#endif
	settingsdialog::UserInterface *userInterfacePage;
	settingsdialog::Tablet *tabletPage;
	settingsdialog::Touch *touchPage;
	const std::initializer_list<
		std::tuple<const char *, QString, QWidget *, bool>>
		panels = {
			{"configure", tr("General"),
			 new settingsdialog::General(m_settings, this), true},
			{"window_", tr("User Interface"),
			 (userInterfacePage =
				  new settingsdialog::UserInterface(m_settings, this)),
			 true},
			{"input-tablet", tr("Tablet"),
			 (tabletPage = new settingsdialog::Tablet(m_settings, this)), true},
			{"input-touchscreen", tr("Touch"),
			 (touchPage = new settingsdialog::Touch(m_settings, this)), true},
			{"tools", tr("Tools"), new settingsdialog::Tools(m_settings, this),
			 true},
			{"network-modem", tr("Network"),
			 new settingsdialog::Network(m_settings, this), true},
			{"dialog-information", tr("Notifications"),
			 new settingsdialog::Notifications(m_settings, this), true},
			{"network-server-database", tr("Servers"),
			 new settingsdialog::Servers(m_settings, singleSession, this),
			 showServers},
			{"input-keyboard", tr("Shortcuts"),
			 new settingsdialog::Shortcuts(m_settings, this), true},
			{"flag", tr("Parental Controls"),
			 new settingsdialog::ParentalControls(m_settings, this), true}};

	auto *buttonLayout = new QVBoxLayout;
	buttonLayout->setContentsMargins(0, 0, 0, 0);
	buttonLayout->setSpacing(0);

	m_stack = new QStackedWidget;
	m_stack->setContentsMargins(0, 0, 0, 0);
	buttonLayout->addWidget(m_stack, 1);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
	const auto l =
		style()->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, this);
	const auto t =
		style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing, nullptr, this);
	const auto r =
		style()->pixelMetric(QStyle::PM_LayoutRightMargin, nullptr, this);
	const auto b =
		style()->pixelMetric(QStyle::PM_LayoutBottomMargin, nullptr, this);
	buttons->setContentsMargins(l, t, r, b);
	buttonLayout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	m_group = new QButtonGroup(this);
	auto first = true;
	int iconSize = style()->pixelMetric(QStyle::PM_ToolBarIconSize);
	for(const auto &[icon, title, panel, visible] : panels) {
		// Gotta use a QToolButton instead of just adding action directly
		// because there is no way to get a plain action to extend the width
		// of the toolbar and it looks ridiculous
		auto *button = new QToolButton;
		button->setIcon(QIcon::fromTheme(icon));
		button->setText(title);
		button->setToolButtonStyle(
			vertical ? Qt::ToolButtonTextBesideIcon
					 : Qt::ToolButtonTextUnderIcon);
		button->setSizePolicy(
			vertical ? QSizePolicy::Expanding : QSizePolicy::Fixed,
			vertical ? QSizePolicy::Fixed : QSizePolicy::Expanding);
		button->setCheckable(true);
		button->setChecked(first);
		button->setAutoRaise(true);
		button->setIconSize(QSize(iconSize, iconSize));

		menuLayout->addWidget(button);
		m_group->addButton(button);

		if(!visible) {
			button->hide();
		}

		button->setProperty("panel", QVariant::fromValue(panel));
		connect(
			button, &QToolButton::toggled, this,
			[this, buttons, panelToActivate = panel](bool checked) {
				if(checked) {
					setUpdatesEnabled(false);
					activatePanel(panelToActivate, buttons);
					setUpdatesEnabled(true);
				}
			});
		addPanel(panel);
		first = false;
	}

	tabletPage->createButtons(buttons);
	touchPage->createButtons(buttons);
	connect(
		userInterfacePage,
		&settingsdialog::UserInterface::scalingDialogRequested, this,
		&SettingsDialog::scalingDialogRequested);
	connect(
		tabletPage, &settingsdialog::Tablet::tabletTesterRequested, this,
		&SettingsDialog::tabletTesterRequested);
	connect(
		touchPage, &settingsdialog::Touch::touchTesterRequested, this,
		&SettingsDialog::touchTesterRequested);

	menuLayout->addStretch();

	QBoxLayout *layout = new QBoxLayout(
		vertical ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom, this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addLayout(buttonLayout, 1);
	layout->insertWidget(menuFirst ? 0 : 1, menuScroll);
}

SettingsDialog::~SettingsDialog()
{
	m_settings.trySubmit();
	m_settings.scalingSettings()->sync();
}

void SettingsDialog::showUserInterfacePage()
{
	for(QAbstractButton *button : m_group->buttons()) {
		QWidget *panel = button->property("panel").value<QWidget *>();
		if(qobject_cast<settingsdialog::UserInterface *>(panel)) {
			button->click();
		}
	}
}

void SettingsDialog::initiateFixShortcutConflicts()
{
	settingsdialog::Shortcuts *shortcuts = activateShortcutsPanel();
	if(shortcuts) {
		shortcuts->initiateFixShortcutConflicts();
	}
}

void SettingsDialog::initiateBrushShortcutChange(int presetId)
{
	settingsdialog::Shortcuts *shortcuts = activateShortcutsPanel();
	if(shortcuts) {
		shortcuts->initiateBrushShortcutChange(presetId);
	}
}

settingsdialog::Shortcuts *SettingsDialog::activateShortcutsPanel()
{
	for(QAbstractButton *button : m_group->buttons()) {
		QWidget *panel = button->property("panel").value<QWidget *>();
		settingsdialog::Shortcuts *shortcuts =
			qobject_cast<settingsdialog::Shortcuts *>(panel);
		if(shortcuts) {
			button->click();
			return shortcuts;
		}
	}
	return nullptr;
}

void SettingsDialog::activatePanel(QWidget *panel, QDialogButtonBox *buttons)
{
	QAbstractButton *closeButton = buttons->button(QDialogButtonBox::Close);
	for(QAbstractButton *button : buttons->buttons()) {
		if(button != closeButton) {
			button->hide();
		}
	}
	m_stack->setCurrentWidget(panel);

	settingsdialog::Tablet *tablet =
		qobject_cast<settingsdialog::Tablet *>(panel);
	if(tablet) {
		tablet->showButtons();
	}

	settingsdialog::Touch *touch = qobject_cast<settingsdialog::Touch *>(panel);
	if(touch) {
		touch->showButtons();
	}
}

void SettingsDialog::addPanel(QWidget *panel)
{
	m_stack->addWidget(panel);
}

} // namespace dialogs
