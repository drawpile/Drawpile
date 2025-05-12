// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog.h"
#include "cmake-config/config.h"
#include "desktop/dialogs/addserverdialog.h"
#include "desktop/dialogs/startdialog/browse.h"
#include "desktop/dialogs/startdialog/create.h"
#include "desktop/dialogs/startdialog/host.h"
#include "desktop/dialogs/startdialog/join.h"
#include "desktop/dialogs/startdialog/links.h"
#include "desktop/dialogs/startdialog/page.h"
#include "desktop/dialogs/startdialog/welcome.h"
#include "desktop/filewrangler.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/utils/recents.h"
#include "desktop/utils/widgetutils.h"
#include <QAction>
#include <QButtonGroup>
#include <QDate>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QMetaEnum>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <functional>
#ifndef __EMSCRIPTEN__
#	include "desktop/dialogs/startdialog/recent.h"
#	include "desktop/dialogs/startdialog/updatenotice.h"
#endif

namespace dialogs {

namespace {

static constexpr QFlags<Qt::WindowType> WINDOW_HINTS =
	Qt::CustomizeWindowHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint;

struct EntryDefinition {
	QString icon;
	QString title;
	QString toolTip;
	startdialog::Page *page;
};

struct LinkDefinition {
	QString icon;
	QString title;
	QString toolTip;
	QUrl url;
};

}

StartDialog::StartDialog(bool smallScreenMode, QWidget *parent)
	: QDialog{parent, WINDOW_HINTS}
#ifndef __EMSCRIPTEN__
	, m_initialUpdateDelayTimer{new QTimer{this}}
	, m_news{dpApp().state(), this}
#endif
{
	setWindowTitle(tr("Start"));
	setWindowModality(Qt::WindowModal);

#ifdef Q_OS_MACOS
	bool vertical = false;
	bool menuFirst = true;
#elif defined(Q_OS_ANDROID)
	bool vertical = false;
	bool menuFirst = false;
#else
	bool vertical = !smallScreenMode;
	bool menuFirst = !smallScreenMode;
#endif

	QVBoxLayout *layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	setLayout(layout);

#ifndef __EMSCRIPTEN__
	m_updateNotice = new startdialog::UpdateNotice;
	layout->addWidget(m_updateNotice);
#endif

	QBoxLayout *mainLayout = new QBoxLayout(
		vertical ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
	mainLayout->setContentsMargins(0, 0, 0, 0);
	mainLayout->setSpacing(0);
	layout->addLayout(mainLayout);

	QWidget *menu = new QWidget;
	menu->setSizePolicy(
		vertical ? QSizePolicy::Fixed : QSizePolicy::MinimumExpanding,
		QSizePolicy::MinimumExpanding);
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
		vertical ? Qt::ScrollBarAsNeeded : Qt ::ScrollBarAlwaysOff);
	menuScroll->setContentsMargins(0, 0, 0, 0);
	menuScroll->setWidgetResizable(true);
	menuScroll->setWidget(menu);
	mainLayout->insertWidget(menuFirst ? mainLayout->count() : 0, menuScroll);

	startdialog::Welcome *welcomePage = new startdialog::Welcome{this};
	startdialog::Join *joinPage = new startdialog::Join{this};
	startdialog::Browse *browsePage = new startdialog::Browse{this};
	startdialog::Host *hostPage = new startdialog::Host{this};
	startdialog::Create *createPage = new startdialog::Create{this};
#ifndef __EMSCRIPTEN__
	startdialog::Recent *recentPage = new startdialog::Recent{this};
#endif

	EntryDefinition defs[Entry::Count];
	defs[Entry::Welcome] = {
		"love", tr("Welcome"), tr("News and updates"), welcomePage};
	defs[Entry::Join] = {
		"network-connect", tr("Join Session"),
		tr("Connect to a drawing session directly"), joinPage};
	defs[Entry::Browse] = {
		"edit-find", tr("Browse Sessions"),
		tr("Browse publicly listed drawing sessions"), browsePage};
	defs[Entry::Host] = {
		"network-server", tr("Host Session"),
		tr("Share your canvas with others"), hostPage};
	defs[Entry::Create] = {
		"document-new", tr("New Canvas"), tr("Create a new, empty canvas"),
		createPage};
	defs[Entry::Open] = {
		"document-open", tr("Open File"), tr("Open an image file"), nullptr};
#ifndef __EMSCRIPTEN__
	defs[Entry::Recent] = {
		"document-open-recent", tr("Recent Files"),
		tr("Reopen a recently used file"), recentPage};
#endif
	defs[Entry::Layouts] = {
		"window_", tr("Layouts"), tr("Choose application layout"), nullptr};
	defs[Entry::Preferences] = {
		"configure", tr("Preferences"), tr("Change application settings"),
		nullptr};

	QVBoxLayout *contentLayout = new QVBoxLayout;
	contentLayout->setSpacing(
		style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing));
	contentLayout->setContentsMargins(
		style()->pixelMetric(QStyle::PM_LayoutLeftMargin, nullptr, this),
		style()->pixelMetric(QStyle::PM_LayoutTopMargin, nullptr, this),
		style()->pixelMetric(QStyle::PM_LayoutRightMargin, nullptr, this),
		style()->pixelMetric(QStyle::PM_LayoutBottomMargin, nullptr, this));
	mainLayout->insertLayout(
		menuFirst ? mainLayout->count() : 0, contentLayout, 1);

	m_stack = new QStackedWidget;
	m_stack->setContentsMargins(0, 0, 0, 0);
	contentLayout->addWidget(m_stack, 1);

	QHBoxLayout *buttonLayout = new QHBoxLayout;
	buttonLayout->setSpacing(
		style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing));
	buttonLayout->setContentsMargins(0, 0, 0, 0);
	contentLayout->addLayout(buttonLayout);

	m_recordButton =
		new QPushButton{QIcon::fromTheme("media-record"), tr("Record")};
	m_recordButton->setCheckable(true);
	m_recordButton->hide();
	buttonLayout->addWidget(m_recordButton);

	m_addServerButton =
		new QPushButton{QIcon::fromTheme("list-add"), tr("Add Server")};
	m_addServerButton->hide();
	buttonLayout->addWidget(m_addServerButton);

	m_saveLoadButton =
		new QPushButton(QIcon::fromTheme("document-import"), tr("Save/Load"));
	m_saveLoadButton->hide();
	buttonLayout->addWidget(m_saveLoadButton);

	QMenu *saveLoadMenu = new QMenu(m_saveLoadButton);
	m_saveLoadButton->setMenu(saveLoadMenu);

	QAction *resetAction = saveLoadMenu->addAction(
		QIcon::fromTheme("view-refresh"), tr("Reload defaults…"));
	connect(resetAction, &QAction::triggered, this, &StartDialog::triggerReset);

	QAction *loadAction = saveLoadMenu->addAction(
		QIcon::fromTheme("document-open"), tr("Load settings…"));
	connect(loadAction, &QAction::triggered, this, &StartDialog::triggerLoad);

	QAction *saveAction = saveLoadMenu->addAction(
		QIcon::fromTheme("document-save-as"), tr("Save settings…"));
	connect(saveAction, &QAction::triggered, this, &StartDialog::triggerSave);

	QAction *importAction = saveLoadMenu->addAction(
		QIcon::fromTheme("document-import"), tr("Import from file…"));
	connect(
		importAction, &QAction::triggered, this, &StartDialog::triggerImport);

	QAction *exportAction = saveLoadMenu->addAction(
		QIcon::fromTheme("document-export"), tr("Export to file…"));
	connect(
		exportAction, &QAction::triggered, this, &StartDialog::triggerExport);

#ifndef __EMSCRIPTEN__
	m_checkForUpdatesButton = new QPushButton{
		QIcon::fromTheme("update-none"), tr("Check for Updates")};
	m_checkForUpdatesButton->setEnabled(false);
	m_checkForUpdatesButton->hide();
	buttonLayout->addWidget(m_checkForUpdatesButton);
#endif

	QDialogButtonBox *buttons = new QDialogButtonBox;
	buttonLayout->addWidget(buttons);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	m_okButton = buttons->addButton(QDialogButtonBox::Ok);
	m_cancelButton = buttons->addButton(QDialogButtonBox::Cancel);
	m_closeButton = buttons->addButton(QDialogButtonBox::Close);

	QButtonGroup *group = new QButtonGroup{this};
	int iconSize = style()->pixelMetric(QStyle::PM_ToolBarIconSize);
	for(int i = 0; i < Entry::Count; ++i) {
		const EntryDefinition &def = defs[i];

		QToolButton *button = new QToolButton;
		button->setIcon(QIcon::fromTheme(def.icon));
		button->setText(def.title);
		button->setToolTip(def.toolTip);
		button->setSizePolicy(
			vertical ? QSizePolicy::Expanding : QSizePolicy::Fixed,
			vertical ? QSizePolicy::Fixed : QSizePolicy::Expanding);
		button->setToolButtonStyle(
			vertical ? Qt::ToolButtonTextBesideIcon
					 : Qt::ToolButtonTextUnderIcon);
		button->setAutoRaise(true);
		button->setIconSize(QSize(iconSize, iconSize));
		m_buttons[i] = button;

		if(def.page) {
			def.page->setProperty(ENTRY_PROPERTY_KEY, i);
			button->setCheckable(true);
			m_stack->addWidget(def.page);
			connect(
				button, &QToolButton::toggled, this,
				std::bind(
					&StartDialog::entryToggled, this, def.page,
					std::placeholders::_1));
		} else {
			connect(
				button, &QToolButton::clicked, this,
				std::bind(&StartDialog::entryClicked, this, Entry(i)));
		}

		if(i == Entry::Layouts) {
			menuLayout->addStretch();
		}

		menuLayout->addWidget(button);
		group->addButton(button);
	}

	m_linksSeparator = new QFrame;
	m_linksSeparator->setForegroundRole(QPalette::Dark);
	m_linksSeparator->setFrameShape(vertical ? QFrame::VLine : QFrame::HLine);
	mainLayout->insertWidget(
		menuFirst ? mainLayout->count() : 0, m_linksSeparator);

	m_links = new startdialog::Links(vertical);
	mainLayout->insertWidget(menuFirst ? mainLayout->count() : 0, m_links);

	connect(
		m_addServerButton, &QPushButton::clicked, this,
		&StartDialog::addListServer);
	connect(
		m_recordButton, &QPushButton::toggled, this,
		&StartDialog::toggleRecording);
#ifndef __EMSCRIPTEN__
	connect(
		m_checkForUpdatesButton, &QPushButton::clicked, this,
		&StartDialog::checkForUpdates);
#endif
	connect(
		m_okButton, &QAbstractButton::clicked, this, &StartDialog::okClicked);

	connect(
		welcomePage, &startdialog::Welcome::showButtons, this,
		&StartDialog::showWelcomeButtons);
	connect(
		welcomePage, &startdialog::Welcome::linkActivated, this,
		&StartDialog::followLink);

	connect(
		joinPage, &startdialog::Join::showButtons, this,
		&StartDialog::showJoinButtons);
	connect(
		joinPage, &startdialog::Join::enableJoin, m_okButton,
		&QWidget::setEnabled);
	connect(
		joinPage, &startdialog::Join::join, this, &StartDialog::joinRequested);
	connect(
		this, &StartDialog::joinAddressSet, joinPage,
		&startdialog::Join::setAddress);

	connect(
		browsePage, &startdialog::Browse::hideLinks, this,
		&StartDialog::hideLinks);
	connect(
		browsePage, &startdialog::Browse::showButtons, this,
		&StartDialog::showBrowseButtons);
	connect(
		browsePage, &startdialog::Browse::enableJoin, m_okButton,
		&QWidget::setEnabled);
	connect(
		browsePage, &startdialog::Browse::join, this,
		&StartDialog::joinRequested);
	connect(
		browsePage, &startdialog::Browse::addListServerUrlRequested, this,
		&StartDialog::addListServerUrl);

	connect(
		hostPage, &startdialog::Host::hideLinks, this, &StartDialog::hideLinks);
	connect(
		hostPage, &startdialog::Host::showButtons, this,
		&StartDialog::showHostButtons);
	connect(
		hostPage, &startdialog::Host::host, this, &StartDialog::hostRequested);
	connect(
		this, &StartDialog::hostPageEnabled, hostPage,
		&startdialog::Host::setEnabled);
	connect(
		hostPage, &startdialog::Host::switchToJoinPageRequested, this, [this] {
			showPage(Entry::Join);
		});

	connect(
		createPage, &startdialog::Create::showButtons, this,
		&StartDialog::showCreateButtons);
	connect(
		createPage, &startdialog::Create::enableCreate, m_okButton,
		&QWidget::setEnabled);
	connect(
		createPage, &startdialog::Create::create, this, &StartDialog::create);

#ifndef __EMSCRIPTEN__
	connect(
		recentPage, &startdialog::Recent::openPath, this,
		&StartDialog::openRecent);
#endif

	setMinimumSize(600, 350);
	setSmallScreenMode(smallScreenMode);

	const desktop::settings::Settings &settings = dpApp().settings();
	QSize lastSize = settings.lastStartDialogSize();
	resize(lastSize.isValid() ? lastSize : QSize{820, 450});

	connect(
		m_stack, &QStackedWidget::currentChanged, this,
		&StartDialog::rememberLastPage);

#ifdef __EMSCRIPTEN__
	welcomePage->showStandaloneText();
#else
	// Delay showing of the update notice to make it more noticeable. It'll jerk
	// the whole UI if it comes in after a second, making it hard to miss.
	m_initialUpdateDelayTimer->setTimerType(Qt::CoarseTimer);
	m_initialUpdateDelayTimer->setSingleShot(true);
	m_initialUpdateDelayTimer->setInterval(1000);
	connect(
		m_initialUpdateDelayTimer, &QTimer::timeout, this,
		&StartDialog::initialUpdateDelayFinished);
	m_initialUpdateDelayTimer->start();

	connect(
		&m_news, &utils::News::fetchInProgress, this,
		&StartDialog::updateCheckForUpdatesButton);
	connect(
		&m_news, &utils::News::newsAvailable, welcomePage,
		&startdialog::Welcome::setNews);
	connect(
		&m_news, &utils::News::updateAvailable, this, &StartDialog::setUpdate);
	updateCheckForUpdatesButton(false);

	if(!settings.welcomePageShown()) {
		welcomePage->showFirstStartText();
	} else if(settings.updateCheckEnabled()) {
		m_news.check();
	} else {
		m_news.checkExisting();
	}
#endif
}

void StartDialog::setActions(const Actions &actions)
{
	for(QShortcut *shortcut : m_shortcuts) {
		delete shortcut;
	}
	m_shortcuts.clear();

	for(int i = 0; i < Entry::Count; ++i) {
		const QAction *action = actions.entries[i];
		if(action) {
			for(const QKeySequence &keySequence : action->shortcuts()) {
				QShortcut *shortcut = new QShortcut{keySequence, this};
				connect(
					shortcut, &QShortcut::activated, m_buttons[i],
					&QAbstractButton::click);
				m_shortcuts.append(shortcut);
			}
		}
	}

	const QAction *hostAction = actions.entries[Entry::Host];
	if(hostAction) {
		emit hostSessionEnabled(hostAction->isEnabled());
	}
}

void StartDialog::showPage(Entry entry)
{
	if(entry >= 0 && entry < Entry::Count) {
		m_buttons[entry]->click();
	} else {
		guessPage();
	}
}

void StartDialog::autoJoin(const QUrl &url, const QString &autoRecordPath)
{
	{
		QSignalBlocker blocker(m_recordButton);
		m_recordButton->setChecked(!autoRecordPath.isEmpty());
		m_recordingFilename = autoRecordPath;
	}
	emit joinAddressSet(url.toString());
	showPage(Entry::Join);
	emit joinRequested(url);
}

#ifndef __EMSCRIPTEN__
void StartDialog::checkForUpdates()
{
	m_news.forceCheck(CHECK_FOR_UPDATES_DELAY_MSEC);
}
#endif

void StartDialog::setSmallScreenMode(bool smallScreenMode)
{
	m_buttons[Entry::Layouts]->setEnabled(!smallScreenMode);
	m_buttons[Entry::Layouts]->setVisible(!smallScreenMode);
}

void StartDialog::resizeEvent(QResizeEvent *event)
{
	QDialog::resizeEvent(event);
	dpApp().settings().setLastStartDialogSize(size());
}

void StartDialog::addListServer()
{
	addListServerUrl(QString{});
}

void StartDialog::addListServerUrl(const QUrl &url)
{
	m_addServerButton->setEnabled(false);

	AddServerDialog *dlg = new AddServerDialog{this};
	dlg->setAttribute(Qt::WA_DeleteOnClose);

	connect(dlg, &QObject::destroyed, [this]() {
		m_addServerButton->setEnabled(true);
	});

	if(!url.isEmpty()) {
		dlg->query(url);
	}

	dlg->show();
}

void StartDialog::toggleRecording(bool checked)
{
	if(checked) {
		m_recordingFilename = FileWrangler{this}.getSaveRecordingPath();
		if(m_recordingFilename.isEmpty()) {
			m_recordButton->setChecked(false);
		}
	} else {
		m_recordingFilename.clear();
	}
}

void StartDialog::triggerReset()
{
	startdialog::Host *hostPage =
		qobject_cast<startdialog::Host *>(m_currentPage);
	if(hostPage) {
		hostPage->triggerReset();
	}
}

void StartDialog::triggerLoad()
{
	startdialog::Host *hostPage =
		qobject_cast<startdialog::Host *>(m_currentPage);
	if(hostPage) {
		hostPage->triggerLoad();
	}
}

void StartDialog::triggerSave()
{
	startdialog::Host *hostPage =
		qobject_cast<startdialog::Host *>(m_currentPage);
	if(hostPage) {
		hostPage->triggerSave();
	}
}

void StartDialog::triggerImport()
{
	startdialog::Host *hostPage =
		qobject_cast<startdialog::Host *>(m_currentPage);
	if(hostPage) {
		hostPage->triggerImport();
	}
}

void StartDialog::triggerExport()
{
	startdialog::Host *hostPage =
		qobject_cast<startdialog::Host *>(m_currentPage);
	if(hostPage) {
		hostPage->triggerExport();
	}
}

#ifndef __EMSCRIPTEN__
void StartDialog::updateCheckForUpdatesButton(bool inProgress)
{
	QString text;
	if(inProgress) {
		text = tr("Checking…");
	} else {
		QDate date = m_news.lastCheck();
		if(date.isValid()) {
			long long days = date.daysTo(QDate::currentDate());
			if(days == 0) {
				text = tr("Last check: today.");
			} else {
				text = tr("Last check: %n day(s) ago.", nullptr, days);
			}
		} else {
			text = tr("Last check: never.");
		}
	}
	m_checkForUpdatesButton->setDisabled(inProgress);
	m_checkForUpdatesButton->setToolTip(text);
}
#endif

void StartDialog::hideLinks()
{
	m_linksSeparator->hide();
	m_links->hide();
}

void StartDialog::showWelcomeButtons()
{
#ifndef __EMSCRIPTEN__
	m_checkForUpdatesButton->show();
#endif
	m_cancelButton->hide();
	m_closeButton->show();
}

void StartDialog::showJoinButtons()
{
#ifndef __EMSCRIPTEN__
	m_recordButton->show();
	m_recordButton->setEnabled(true);
#endif
	m_okButton->setText(tr("Join"));
	m_okButton->show();
}

void StartDialog::showBrowseButtons()
{
	showJoinButtons();
	m_addServerButton->show();
	m_addServerButton->setEnabled(true);
}

void StartDialog::showHostButtons()
{
	m_okButton->setText(tr("Host"));
	m_okButton->show();
	m_okButton->setEnabled(true);
	m_saveLoadButton->show();
	m_saveLoadButton->setEnabled(true);
}

void StartDialog::showCreateButtons()
{
	m_okButton->setText(tr("Create"));
	m_okButton->show();
}

void StartDialog::okClicked()
{
	if(m_currentPage) {
		m_currentPage->accept();
	}
}

void StartDialog::followLink(const QString &fragment)
{
	if(fragment.compare("autoupdate", Qt::CaseInsensitive) == 0) {
		dpApp().settings().setUpdateCheckEnabled(true);
#ifndef __EMSCRIPTEN__
		m_checkForUpdatesButton->click();
#endif
	} else if(fragment.compare("checkupdates", Qt::CaseInsensitive) == 0) {
#ifndef __EMSCRIPTEN__
		m_checkForUpdatesButton->click();
#endif
	} else {
		QMetaEnum entries = QMetaEnum::fromType<Entry>();
		for(int i = 0; i < Entry::Count; ++i) {
			QString key = QString::fromUtf8(entries.key(i));
			if(fragment.compare(key, Qt::CaseInsensitive) == 0) {
				showPage(Entry(entries.value(i)));
				return;
			}
		}
		qWarning("Unknown link '%s'", qUtf8Printable(fragment));
	}
}

void StartDialog::joinRequested(const QUrl &url)
{
	QString listServer = QUrlQuery{url}.queryItemValue("list-server");
	if(listServer.isEmpty()) {
		addRecentHost(url, true);
		emit join(url, m_recordingFilename);
	} else {
		showPage(Entry::Browse);
		emit joinAddressSet(QString{});
		addListServerUrl(QUrl{listServer});
	}
}

void StartDialog::hostRequested(const HostParams &params)
{
	if(params.rememberAddress && !params.address.isEmpty()) {
		addRecentHost(params.address, false);
	}
	emit host(params);
}

void StartDialog::rememberLastPage(int i)
{
	QWidget *page = m_stack->widget(i);
	if(page) {
		bool ok;
		int entry = page->property(ENTRY_PROPERTY_KEY).toInt(&ok);
		if(ok && entry >= 0 && entry < Entry::Count) {
			desktop::settings::Settings &settings = dpApp().settings();
			settings.setLastStartDialogPage(entry);
			settings.setLastStartDialogDateTime(
				QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
		}
	}
}

#ifndef __EMSCRIPTEN__
void StartDialog::initialUpdateDelayFinished()
{
	m_initialUpdateDelayTimer->deleteLater();
	m_initialUpdateDelayTimer = nullptr;
	if(m_update.isValid()) {
		m_updateNotice->setUpdate(&m_update);
	}
}

void StartDialog::setUpdate(const utils::News::Update &update)
{
	m_update = update;
	if(!m_initialUpdateDelayTimer || !m_initialUpdateDelayTimer->isActive()) {
		m_updateNotice->setUpdate(&m_update);
	}
}
#endif

void StartDialog::entryClicked(Entry entry)
{
	switch(entry) {
	case Entry::Open:
		emit openFile();
		break;
	case Entry::Layouts:
		emit layouts();
		break;
	case Entry::Preferences:
		emit preferences();
		break;
	default:
		break;
	}
}

void StartDialog::entryToggled(startdialog::Page *page, bool checked)
{
	if(checked) {
		setUpdatesEnabled(false);
		m_linksSeparator->show();
		m_links->show();
		m_addServerButton->hide();
		m_recordButton->hide();
		m_saveLoadButton->hide();
#ifndef __EMSCRIPTEN__
		m_checkForUpdatesButton->hide();
#endif
		m_okButton->hide();
		m_cancelButton->show();
		m_closeButton->hide();
		m_addServerButton->setEnabled(false);
		m_recordButton->setEnabled(false);
		m_saveLoadButton->setEnabled(false);
		m_okButton->setEnabled(false);
		m_currentPage = page;
		m_stack->setCurrentWidget(page);
		page->activate();
		setUpdatesEnabled(true);
	}
}

void StartDialog::guessPage()
{
	const desktop::settings::Settings &settings = dpApp().settings();
	if(settings.welcomePageShown()) {
		int lastPage = settings.lastStartDialogPage();
		QDateTime lastDateTime = QDateTime::fromString(
			settings.lastStartDialogDateTime(), Qt::ISODate);
		bool lastPageValid =
			lastPage >= 0 && lastPage < Entry::Count &&
			lastDateTime.isValid() &&
			lastDateTime.secsTo(QDateTime::currentDateTimeUtc()) <
				MAX_LAST_PAGE_REMEMBER_SECS &&
			m_buttons[lastPage]->isCheckable();
		showPage(lastPageValid ? Entry(lastPage) : Entry::Welcome);
	} else {
		showPage(Entry::Welcome);
	}
}

void StartDialog::addRecentHost(const QUrl &url, bool join)
{
	if(!url.isValid()) {
		return;
	}

	bool webSocket;
	QString scheme = url.scheme();
	if(scheme.compare(QStringLiteral("drawpile"), Qt::CaseInsensitive) == 0) {
		QUrlQuery query(url);
		webSocket = query.hasQueryItem(QStringLiteral("w")) ||
					query.hasQueryItem(QStringLiteral("W"));
	} else if(scheme.compare(QStringLiteral("wss"), Qt::CaseInsensitive) == 0) {
		webSocket = true;
	} else {
		return;
	}

	int port = webSocket ? -1 : url.port();
	dpApp().recents().addHost(
		url.host(), port > 0 ? port : cmake_config::proto::port(), join, !join,
		webSocket);
}

}
