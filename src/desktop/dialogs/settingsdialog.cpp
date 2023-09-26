// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/settingsdialog.h"
#include "desktop/dialogs/settingsdialog/canvasshortcuts.h"
#include "desktop/dialogs/settingsdialog/general.h"
#include "desktop/dialogs/settingsdialog/input.h"
#include "desktop/dialogs/settingsdialog/network.h"
#include "desktop/dialogs/settingsdialog/notifications.h"
#include "desktop/dialogs/settingsdialog/parentalcontrols.h"
#include "desktop/dialogs/settingsdialog/servers.h"
#include "desktop/dialogs/settingsdialog/shortcuts.h"
#include "desktop/dialogs/settingsdialog/tools.h"
#include "desktop/dialogs/settingsdialog/userinterface.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include <QAction>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QStackedWidget>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace dialogs {

SettingsDialog::SettingsDialog(QWidget *parent)
	: QDialog(parent)
	, m_settings(dpApp().settings())
{
	setWindowTitle(tr("Preferences"));
	resize(800, 600);

	setWindowModality(Qt::ApplicationModal);

	auto *layout = new QHBoxLayout(this);

	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	// TODO: Needs to use NSToolbar for macOS to get the correct integration
	// with the title bar
	auto *menu = new QToolBar;
	menu->setMovable(false);
	menu->setFloatable(false);
	menu->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding);
	menu->setOrientation(Qt::Vertical);
	menu->setBackgroundRole(QPalette::Midlight);
	menu->setAutoFillBackground(true);

	layout->addWidget(menu);

	const std::initializer_list<std::tuple<const char *, QString, QWidget *>>
		panels = {
			{"configure", tr("General"),
			 new settingsdialog::General(m_settings, this)},
			{"window_", tr("User Interface"),
			 new settingsdialog::UserInterface(m_settings, this)},
			{"dialog-input-devices", tr("Input"),
			 new settingsdialog::Input(m_settings, this)},
			{"tools", tr("Tools"), new settingsdialog::Tools(m_settings, this)},
			{"network-modem", tr("Network"),
			 new settingsdialog::Network(m_settings, this)},
			{"dialog-information", tr("Notifications"),
			 new settingsdialog::Notifications(m_settings, this)},
			{"network-server-database", tr("Servers"),
			 new settingsdialog::Servers(m_settings, this)},
			{"input-keyboard", tr("Shortcuts"),
			 new settingsdialog::Shortcuts(m_settings, this)},
			{"edit-rename", tr("Canvas Shortcuts"),
			 new settingsdialog::CanvasShortcuts(m_settings, this)},
			{"flag", tr("Parental Controls"),
			 new settingsdialog::ParentalControls(m_settings, this)}};

	auto *buttonLayout = new QVBoxLayout;
	buttonLayout->setContentsMargins(0, 0, 0, 0);
	buttonLayout->setSpacing(0);
	layout->addLayout(buttonLayout, 1);

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
	for(const auto &[icon, title, panel] : panels) {
		// Gotta use a QToolButton instead of just adding action directly
		// because there is no way to get a plain action to extend the width
		// of the toolbar and it looks ridiculous
		auto *button = new QToolButton;
		button->setIcon(QIcon::fromTheme(icon));
		button->setText(title);
		button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		button->setCheckable(true);
		button->setChecked(first);

		menu->addWidget(button);
		m_group->addButton(button);

		button->setProperty("panel", QVariant::fromValue(panel));
		connect(
			button, &QToolButton::toggled, this,
			[=, panel = panel](bool checked) {
				if(checked) {
					setUpdatesEnabled(false);
					activatePanel(panel);
					setUpdatesEnabled(true);
				}
			});
		addPanel(panel);
		first = false;
	}
}

void SettingsDialog::activateShortcutsPanel()
{
	for(QAbstractButton *button : m_group->buttons()) {
		QWidget *panel = button->property("panel").value<QWidget *>();
		if(qobject_cast<settingsdialog::Shortcuts *>(panel)) {
			button->click();
		}
	}
}

void SettingsDialog::activatePanel(QWidget *panel)
{
	m_stack->setCurrentWidget(panel);
}

void SettingsDialog::addPanel(QWidget *panel)
{
	m_stack->addWidget(panel);
}

} // namespace dialogs
