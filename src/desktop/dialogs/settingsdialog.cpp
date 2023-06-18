// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/dialogs/settingsdialog.h"
#include "desktop/dialogs/settingsdialog/parentalcontrols.h"
#include "desktop/dialogs/settingsdialog/general.h"
#include "desktop/dialogs/settingsdialog/input.h"
#include "desktop/dialogs/settingsdialog/network.h"
#include "desktop/dialogs/settingsdialog/servers.h"
#include "desktop/dialogs/settingsdialog/shortcuts.h"
#include "desktop/dialogs/settingsdialog/canvasshortcuts.h"
#include "desktop/dialogs/settingsdialog/tools.h"

#include <QAction>
#include <QBoxLayout>
#include <QButtonGroup>
#ifndef Q_OS_MACOS
#include <QDialogButtonBox>
#include <QStackedWidget>
#include <QVBoxLayout>
#endif
#include <QStyle>
#include <QToolBar>
#include <QToolButton>

namespace dialogs {

SettingsDialog::SettingsDialog(QWidget *parent)
	: QDialog(parent, Qt::Window | Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint)
	, m_settings(dpApp().settings())
{
	setWindowTitle(tr("Preferences"));

#ifdef Q_OS_MACOS
	const auto orientation = QBoxLayout::TopToBottom;
	const auto menuStyle = Qt::ToolButtonTextUnderIcon;
	const auto menuOrientation = Qt::Horizontal;
	const auto menuPolicyX = QSizePolicy::MinimumExpanding;
	const auto menuPolicyY = QSizePolicy::Fixed;
	const auto buttonPolicyX = QSizePolicy::Fixed;
#else
	const auto orientation = QBoxLayout::LeftToRight;
	const auto menuStyle = Qt::ToolButtonTextBesideIcon;
	const auto menuOrientation = Qt::Vertical;
	const auto menuPolicyX = QSizePolicy::Fixed;
	const auto menuPolicyY = QSizePolicy::MinimumExpanding;
	const auto buttonPolicyX = QSizePolicy::Expanding;
	setWindowModality(Qt::ApplicationModal);
#endif

	auto *layout = new QBoxLayout(orientation, this);

	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	// TODO: Needs to use NSToolbar for macOS to get the correct integration
	// with the title bar
	auto *menu = new QToolBar;
	menu->setMovable(false);
	menu->setFloatable(false);
	menu->setSizePolicy(menuPolicyX, menuPolicyY);
	menu->setOrientation(menuOrientation);
	menu->setBackgroundRole(QPalette::Midlight);
	menu->setAutoFillBackground(true);

	layout->addWidget(menu);

	const std::initializer_list<std::tuple<const char *, QString, QWidget *>> panels = {
		{ "configure", tr("General"), new settingsdialog::General(m_settings, this) },
		{ "dialog-input-devices", tr("Input"), new settingsdialog::Input(m_settings, this) },
		{ "tools", tr("Tools"), new settingsdialog::Tools(m_settings, this) },
		{ "network-modem", tr("Network"), new settingsdialog::Network(m_settings, this) },
		{ "network-server-database", tr("Servers"), new settingsdialog::Servers(m_settings, this) },
		{ "input-keyboard", tr("Shortcuts"), new settingsdialog::Shortcuts(m_settings, this) },
		{ "edit-rename", tr("Canvas Shortcuts"), new settingsdialog::CanvasShortcuts(m_settings, this) },
		{ "flag", tr("Parental Controls"), new settingsdialog::ParentalControls(m_settings, this) }
	};

#ifdef Q_OS_MACOS
	// On macOS, the preferences should have the same width always but change
	// height as the content shifts, so we do not use a QStackedWidget since
	// that would make the height always the same
	auto width = 0;
#else
	auto *buttonLayout = new QVBoxLayout;
	buttonLayout->setContentsMargins(0, 0, 0, 0);
	buttonLayout->setSpacing(0);
	layout->addLayout(buttonLayout, 1);

	m_stack = new QStackedWidget;
	m_stack->setContentsMargins(0, 0, 0, 0);
	buttonLayout->addWidget(m_stack, 1);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
	const auto l = style()->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, this);
	const auto t = style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing, nullptr, this);
	const auto r = style()->pixelMetric(QStyle::PM_LayoutRightMargin, nullptr, this);
	const auto b = style()->pixelMetric(QStyle::PM_LayoutBottomMargin, nullptr, this);
	buttons->setContentsMargins(l, t, r, b);
	buttonLayout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
#endif

	m_group = new QButtonGroup(this);
	auto first = true;
	for (const auto &[ icon, title, panel ] : panels) {
#ifdef Q_OS_MACOS
		width = qMax(width, panel->sizeHint().width());
#endif
		// Gotta use a QToolButton instead of just adding action directly
		// because there is no way to get a plain action to extend the width
		// of the toolbar and it looks ridiculous
		auto *button = new QToolButton;
		button->setIcon(QIcon::fromTheme(icon));
		button->setText(title);
		button->setToolButtonStyle(menuStyle);
		button->setSizePolicy(buttonPolicyX, QSizePolicy::Fixed);
		button->setCheckable(true);
		button->setChecked(first);

		menu->addWidget(button);
		m_group->addButton(button);

		button->setProperty("panel", QVariant::fromValue(panel));
		connect(button, &QToolButton::toggled, this, [=, panel = panel](bool checked) {
			if (checked) {
				setUpdatesEnabled(false);
				activatePanel(panel);
				setUpdatesEnabled(true);
			}
		});
		addPanel(panel);
		first = false;
	}

#ifdef Q_OS_MACOS
	const auto margins = layout->contentsMargins();
	setFixedWidth(width + margins.left() + margins.right());
#else
	setFixedSize(layout->sizeHint());
#endif
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

#ifdef Q_OS_MACOS
void SettingsDialog::activatePanel(QWidget *panel)
{
	if (m_current) {
		m_current->setHidden(true);
	}
	m_current = panel;
	panel->setHidden(false);
	setFixedHeight(layout()->sizeHint().height());
}

void SettingsDialog::addPanel(QWidget *panel)
{
	static_cast<QBoxLayout *>(layout())->addWidget(panel, 1);
	panel->setHidden(m_current);
	if (!m_current) {
		m_current = panel;
	}
}
#else
void SettingsDialog::activatePanel(QWidget *panel)
{
	m_stack->setCurrentWidget(panel);
}

void SettingsDialog::addPanel(QWidget *panel)
{
	m_stack->addWidget(panel);
}
#endif

bool SettingsDialog::event(QEvent *event)
{
	const auto result = QDialog::event(event);

	switch (event->type()) {
	case QEvent::StyleChange:
	case QEvent::MacSizeChange: {
#ifdef Q_OS_MACOS
		auto width = 0;
		for (const auto *widget : findChildren<const QWidget *>()) {
			width = qMax(width, widget->sizeHint().width());
		}
		const auto margins = layout()->contentsMargins();
		// Width then height to update geometry before getting the size hint for
		// height (this could be superstitious nonsense)
		setFixedWidth(width + margins.left() + margins.right());
		setFixedHeight(layout()->sizeHint().height());
#else
		setFixedSize(layout()->sizeHint());
#endif
		break;
	}
#ifdef Q_OS_MACOS
	case QEvent::LayoutRequest: {
		const auto newHeight = layout()->sizeHint().height();
		if (height() != newHeight) {
			setFixedHeight(newHeight);
		}
		break;
	}
#endif
	default: {}
	}

	return result;
}

} // namespace dialogs
