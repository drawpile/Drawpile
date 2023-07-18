// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/chat/chatwidgetpinnedarea.h"
#include "desktop/chat/chatwidget.h"
#include "libclient/utils/html.h"
#include "libclient/utils/funstuff.h"
#include "desktop/notifications.h"
#include "desktop/main.h"

#include "libclient/canvas/userlist.h"
#include "libclient/drawdance/message.h"
#include "libclient/drawdance/perf.h"

#include <QResizeEvent>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QLabel>
#include <QDateTime>
#include <QTextBlock>
#include <QScrollBar>
#include <QTabBar>
#include <QIcon>
#include <QMenu>
#include <QRegularExpression>
#include <QSignalBlocker>

#ifdef Q_OS_ANDROID
#	include "desktop/chat/chatlineeditandroid.h"
#else
#	include "desktop/chat/chatlineedit.h"
#endif

#define DP_PERF_CONTEXT "chat_widget"

namespace widgets {

struct Chat {
	QTextDocument *doc;
	int lastAppendedId = 0;
	qint64 lastMessageTs = 0;
	int scrollPosition = 0;

	Chat() : doc(nullptr) { }
	explicit Chat(QObject *parent)
		: doc(new QTextDocument(parent))
	{
		doc->setDefaultStyleSheet(
			".sep { background: #4d4d4d }"
			".notification { background: #232629 }"
			".message, .notification {"
				"color: #eff0f1;"
				"margin: 1px 0 1px 0"
			"}"
		    ".alert { background: #66da4453 }"
			".shout { background: #34292c }"
			".shout .tab { background: #da4453 }"
			".action { font-style: italic }"
			".username { font-weight: bold }"
			".trusted { color: #27ae60 }"
			".registered { color: #16a085 }"
			".op { color: #f47750 }"
			".mod { color: #ed1515 }"
			".timestamp { color: #8d8d8d }"
		    ".alert .timestamp { color: #eff0f1 }"
			"a:link { color: #1d99f3 }"
		);
	}

	void appendSeparator(QTextCursor &cursor);
	void appendMessage(int userId, const QString &usernameSpan, const QString &message, bool shout, bool alert);
	void appendMessageCompact(int userId, const QString &usernameSpan, const QString &message, bool shout, bool alert);
	void appendAction(const QString &usernameSpan, const QString &message);
	void appendNotification(const QString &message);
};

struct ChatWidget::Private {
	Private(ChatWidget *parent) : chatbox(parent) { }

	ChatWidget * const chatbox;
	QTextBrowser *view = nullptr;
	ChatLineEdit *myline = nullptr;
	ChatWidgetPinnedArea *pinned = nullptr;
	QTabBar *tabs = nullptr;
	QMenu *externalMenu = nullptr;
	QAction *clearAction = nullptr;
	QAction *compactAction = nullptr;
	QAction *attachAction = nullptr;
	QAction *detachAction = nullptr;

	QList<int> announcedUsers;
	canvas::UserListModel *userlist = nullptr;
	QHash<int, Chat> chats;

	int myId = 0;
	int currentChat = 0;

	bool preserveChat = true;
	bool compactMode = COMPACT_ONLY;
	bool isAttached = true;
	bool wasAtEnd = true;

	QString usernameSpan(int userId);

	bool isAtEnd() const
	{
		return view->verticalScrollBar()->value() == view->verticalScrollBar()->maximum();
	}

	void scrollChatToEnd(int ifCurrentId)
	{
		if(ifCurrentId == tabs->tabData(tabs->currentIndex()).toInt()) {
			scrollToEnd();
		}
	}

	void scrollToEnd()
	{
		view->verticalScrollBar()->setValue(view->verticalScrollBar()->maximum());
	}

	inline Chat &publicChat()
	{
		Q_ASSERT(chats.contains(0));
		return chats[0];
	}

	bool ensurePrivateChatExists(int userId, QObject *parent);

	void updatePreserveModeUi();
};

ChatWidget::ChatWidget(QWidget *parent)
	: QWidget(parent), d(new Private(this))
{
	QVBoxLayout *layout = new QVBoxLayout(this);

	layout->setSpacing(0);
	layout->setContentsMargins(0, 0, 0, 0);

	d->tabs = new QTabBar(this);
	d->tabs->addTab(QString());
	d->tabs->setTabText(0, tr("Public"));
	d->tabs->setAutoHide(true);
	d->tabs->setDocumentMode(true);
	d->tabs->setTabsClosable(true);
	d->tabs->setMovable(true);
	d->tabs->setTabData(0, 0); // context id 0 is used for the public chat

	// The public chat cannot be closed
	if(d->tabs->tabButton(0, QTabBar::LeftSide)) {
		d->tabs->tabButton(0, QTabBar::LeftSide)->deleteLater();
		d->tabs->setTabButton(0, QTabBar::LeftSide, nullptr);
	}
	if(d->tabs->tabButton(0, QTabBar::RightSide)) {
		d->tabs->tabButton(0, QTabBar::RightSide)->deleteLater();
		d->tabs->setTabButton(0, QTabBar::RightSide, nullptr);
	}

	connect(d->tabs, &QTabBar::currentChanged, this, &ChatWidget::chatTabSelected);
	connect(d->tabs, &QTabBar::tabCloseRequested, this, &ChatWidget::chatTabClosed);
	layout->addWidget(d->tabs, 0);

	d->pinned = new ChatWidgetPinnedArea(this);
	layout->addWidget(d->pinned, 0);

	d->view = new QTextBrowser(this);
	d->view->setOpenExternalLinks(true);
	connect(d->view->verticalScrollBar(), &QScrollBar::valueChanged, this, &ChatWidget::scrollBarMoved);

	d->view->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(d->view, &QTextBrowser::customContextMenuRequested, this, &ChatWidget::showChatContextMenu);

	layout->addWidget(d->view, 1);

	d->myline = new ChatLineEdit(this);
	layout->addWidget(d->myline);

	setLayout(layout);

	connect(d->myline, &ChatLineEdit::messageSent, this, &ChatWidget::sendMessage);

	d->chats[0] = Chat(this);
	d->view->setDocument(d->chats[0].doc);

	d->externalMenu = new QMenu{this};

	d->clearAction = d->externalMenu->addAction(tr("Clear"), this, &ChatWidget::clear);

	if(!COMPACT_ONLY) {
		d->compactAction = d->externalMenu->addAction(
			tr("Compact mode"), this, &ChatWidget::setCompactMode);
		d->compactAction->setCheckable(true);
		d->compactAction->setChecked(d->compactMode);
	}

	if(ALLOW_DETACH) {
		d->attachAction = d->externalMenu->addAction(
			tr("Attach"), this, &ChatWidget::attach);
		d->detachAction = d->externalMenu->addAction(
			tr("Detach"), this, &ChatWidget::detachRequested);
	}

	connect(d->externalMenu, &QMenu::aboutToShow, this, &ChatWidget::contextMenuAboutToShow);

	setPreserveMode(false);

	dpApp().settings().bindCompactChat(this, [this](bool compact) {
		if(!COMPACT_ONLY) {
			d->compactMode = compact;
		}
		if(d->compactAction) {
			QSignalBlocker blocker{d->compactAction};
			d->compactAction->setChecked(compact);
		}
	});
}

ChatWidget::~ChatWidget()
{
	delete d;
}

void ChatWidget::setAttached(bool isAttached)
{
	d->isAttached = isAttached;
}

void ChatWidget::Private::updatePreserveModeUi()
{
	const bool preserve = preserveChat && currentChat == 0;

	QString placeholder, color;
	if(preserve) {
		placeholder = tr("Chat (recorded)...");
		color = "#da4453";
	} else {
		placeholder = tr("Chat...");
		color = "#1d99f3";
	}

	// Set placeholder text and window style based on the mode
	myline->setPlaceholderText(placeholder);

	chatbox->setStyleSheet(
		QStringLiteral(
		"QTextEdit, QPlainTextEdit, QLineEdit {"
			"background-color: #232629;"
			"border: none;"
			"color: #eff0f1"
		"}"
		"QPlainTextEdit, QLineEdit {"
			"border-top: 1px solid %1;"
			"padding: 4px"
		"}"
		).arg(color)
	);

}
void ChatWidget::setPreserveMode(bool preservechat)
{
	d->preserveChat = preservechat;
	d->updatePreserveModeUi();
}

void ChatWidget::loggedIn(int myId)
{
	d->myId = myId;
	d->announcedUsers.clear();
}

void ChatWidget::focusInput()
{
	d->myline->setFocus();
}

void ChatWidget::setUserList(canvas::UserListModel *userlist)
{
	 d->userlist = userlist;
}

QMenu *ChatWidget::externalMenu()
{
	return d->externalMenu;
}

void ChatWidget::clear()
{
	Chat &chat = d->chats[d->currentChat];
	chat.doc->clear();
	chat.lastAppendedId = 0;

	// Re-add avatars
	if(d->userlist) {
		for(const auto &u : d->userlist->users()) {
			chat.doc->addResource(
				QTextDocument::ImageResource,
				QUrl(QStringLiteral("avatar://%1").arg(u.id)),
				u.avatar
			);
		}
	}
}

bool ChatWidget::Private::ensurePrivateChatExists(int userId, QObject *parent)
{
	if(userId < 1 || userId > 255) {
		qWarning("ChatWidget::openPrivateChat(%d): Invalid user ID", userId);
		return false;
	}
	if(userId == myId) {
		qWarning("ChatWidget::openPrivateChat(%d): this is me...", userId);
		return false;
	}

	if(!chats.contains(userId)) {
		chats[userId] = Chat(parent);
		const int newTab = tabs->addTab(userlist->getUsername(userId));
		tabs->setTabData(newTab, userId);

		chats[userId].doc->addResource(
			QTextDocument::ImageResource,
			QUrl(QStringLiteral("avatar://%1").arg(userId)),
			userlist->getUserById(userId).avatar
		);
		chats[userId].doc->addResource(
			QTextDocument::ImageResource,
			QUrl(QStringLiteral("avatar://%1").arg(myId)),
			userlist->getUserById(myId).avatar
		);
	}

	return true;
}

void ChatWidget::openPrivateChat(int userId)
{
	if(!d->ensurePrivateChatExists(userId, this))
		return;

	for(int i=d->tabs->count()-1;i>=0;--i) {
		if(d->tabs->tabData(i).toInt() == userId) {
			d->tabs->setCurrentIndex(i);
			break;
		}
	}
}

static QString timestamp()
{
	return QStringLiteral("<span class=ts>%1</span>").arg(
		QDateTime::currentDateTime().toString("HH:mm")
	);
}

QString ChatWidget::Private::usernameSpan(int userId)
{
	const canvas::User user = userlist ? userlist->getUserById(userId) : canvas::User();

	QString userclass;
	if(user.isMod)
		userclass = QStringLiteral("mod");
	else if(user.isOperator)
		userclass = QStringLiteral("op");
	else if(user.isTrusted)
		userclass = QStringLiteral("trusted");
	else if(user.isAuth)
		userclass = QStringLiteral("registered");

	return QStringLiteral("<span class=\"username %1\">%2</span>").arg(
		userclass,
		user.name.isEmpty() ? QStringLiteral("<s>User #%1</s>").arg(userId) : user.name.toHtmlEscaped()
	);
}

void Chat::appendSeparator(QTextCursor &cursor)
{
	cursor.insertHtml(QStringLiteral(
		"<table height=1 width=\"100%\" class=sep><tr><td></td></tr></table>"
		));
}

void Chat::appendMessageCompact(int userId, const QString &usernameSpan, const QString &message, bool shout, bool alert)
{
	Q_UNUSED(userId);

	lastAppendedId = -1;

	QTextCursor cursor(doc);

	cursor.movePosition(QTextCursor::End);

	QString content =
		usernameSpan.isEmpty() ? message : QStringLiteral("%1: %2").arg(usernameSpan, message);

	cursor.insertHtml(QStringLiteral(
		"<table width=\"100%\" class=\"%1\">"
		"<tr>"
			"<td width=3 class=tab></td>"
			"<td>%2</td>"
			"<td class=timestamp align=right>%3</td>"
		"</tr>"
		"</table>"
		).arg(
			alert ? " alert" : shout ? " shout" : "",
			htmlutils::newlineToBr(content),
			timestamp()
		)
	);
}

void Chat::appendMessage(int userId, const QString &usernameSpan, const QString &message, bool shout, bool alert)
{
	QTextCursor cursor(doc);
	cursor.movePosition(QTextCursor::End);

	const qint64 ts = QDateTime::currentMSecsSinceEpoch();

	if(shout || alert) {
		lastAppendedId = -2;

	} else if(lastAppendedId != userId) {
		appendSeparator(cursor);
		lastAppendedId = userId;

	} else if(ts - lastMessageTs < 60000) {
		QTextBlock b = doc->lastBlock().previous();
		cursor.setPosition(b.position() + b.length() - 1);

		cursor.insertHtml(QStringLiteral("<br>"));

		// Using css property "white-space: pre" only works for the first message. Newlines disappear on subsequent messages.
		// Thus the need to manually replace newlines by <br>
		cursor.insertHtml(htmlutils::newlineToBr(message));

		return;
	}

	// We'll have to make do with a very limited subset of HTML and CSS:
	// http://doc.qt.io/qt-5/richtext-html-subset.html
	// Embedding a whole browser engine just to render the chat widget would
	// be excessive.
	cursor.insertHtml(QStringLiteral(
		"<table width=\"100%\" class=\"message%1\">"
		"<tr>"
			"<td width=3 rowspan=2 class=tab></td>"
			"<td width=40 rowspan=2><img src=\"avatar://%2\"></td>"
			"<td>%3</td>"
			"<td class=timestamp align=right>%4</td>"
		"</tr>"
		"<tr>"
			"<td colspan=2>%5</td>"
		"</tr>"
		"</table>"
		).arg(
			alert ? " alert" : shout ? " shout" : "",
			QString::number(userId),
			usernameSpan,
			timestamp(),
			htmlutils::newlineToBr(message)
		)
	);
	lastMessageTs = ts;
}

void Chat::appendAction(const QString &usernameSpan, const QString &message)
{
	QTextCursor cursor(doc);
	cursor.movePosition(QTextCursor::End);

	if(lastAppendedId != -1) {
		appendSeparator(cursor);
		lastAppendedId = -1;
	}
	cursor.insertHtml(QStringLiteral(
		"<table width=\"100%\" class=message>"
		"<tr>"
			"<td width=3 class=tab></td>"
			"<td><span class=action>%1 %2</span></td>"
			"<td class=timestamp align=right>%3</td>"
		"</tr>"
		"</table>"
		).arg(
			usernameSpan,
			message,
			timestamp()
		)
	);
}

void Chat::appendNotification(const QString &message)
{
	QTextCursor cursor(doc);
	cursor.movePosition(QTextCursor::End);

	if(lastAppendedId != 0) {
		appendSeparator(cursor);
		lastAppendedId = 0;
	}

	cursor.insertHtml(QStringLiteral(
		"<table width=\"100%\" class=notification><tr>"
			"<td width=3 class=tab></td>"
			"<td>%1</td>"
			"<td align=right class=timestamp>%2</td>"
		"</tr></table>"
		).arg(
			htmlutils::newlineToBr(message),
			timestamp()
		)
	);
}

void ChatWidget::userJoined(int id, const QString &name)
{
	Q_UNUSED(name);

	if(d->userlist) {
		d->chats[0].doc->addResource(
			QTextDocument::ImageResource,
			QUrl(QStringLiteral("avatar://%1").arg(id)),
			d->userlist->getUserById(id).avatar
		);
		if(d->chats.contains(id)) {
			d->chats[id].doc->addResource(
				QTextDocument::ImageResource,
				QUrl(QStringLiteral("avatar://%1").arg(id)),
				d->userlist->getUserById(id).avatar
			);
		}

	} else {
		qWarning("User #%d logged in, but userlist object not assigned to ChatWidget!", id);
	}

	// The server resends UserJoin messages during session reset.
	// We don't need to see the join messages again.
	if(d->announcedUsers.contains(id))
		return;

	d->announcedUsers << id;
	const QString msg = tr("%1 joined the session").arg(d->usernameSpan(id));
	const bool wasAtEnd = d->isAtEnd();

	d->publicChat().appendNotification(msg);
	if(wasAtEnd)
		d->scrollChatToEnd(0);

	if(d->chats.contains(id)) {
		d->chats[id].appendNotification(msg);
		if(wasAtEnd)
			d->scrollChatToEnd(id);
	}

	notification::playSound(notification::Event::LOGIN);
}

void ChatWidget::userParted(int id)
{
	QString msg = tr("%1 left the session").arg(d->usernameSpan(id));
	const bool wasAtEnd = d->isAtEnd();

	d->publicChat().appendNotification(msg);
	if(wasAtEnd)
		d->scrollChatToEnd(0);

	if(d->chats.contains(id)) {
		d->chats[id].appendNotification(msg);
		if(wasAtEnd)
			d->scrollChatToEnd(id);
	}

	d->announcedUsers.removeAll(id);

	notification::playSound(notification::Event::LOGOUT);
}

void ChatWidget::kicked(const QString &kickedBy)
{
	const bool wasAtEnd = d->isAtEnd();
	d->publicChat().appendNotification(tr("You have been kicked by %1").arg(kickedBy.toHtmlEscaped()));
	if(wasAtEnd)
		d->scrollChatToEnd(0);
}

void ChatWidget::receiveMessage(int sender, int recipient, uint8_t tflags, uint8_t oflags, const QString &message)
{
	Q_UNUSED(tflags);

	const bool wasAtEnd = d->isAtEnd();
	// The server echoes our PMs back to us, in which case we identify the chat box
	// the message belongs to based on the recipient field rather than the sender (which is us)
	const int chatId = recipient > 0 ? (recipient == d->myId ? sender : recipient) : 0;

	if(chatId > 0) {
		if(!d->ensurePrivateChatExists(chatId, this))
			return;
	}

	const QString safetext = htmlutils::linkify(message.toHtmlEscaped());

	Q_ASSERT(d->chats.contains(chatId));
	Chat &chat = d->chats[chatId];

	bool isOp = d->userlist && d->userlist->isOperator(sender);
	bool isValidAlert = isOp && oflags & DP_MSG_CHAT_OFLAGS_ALERT;
	bool isValidShout = isOp && oflags & DP_MSG_CHAT_OFLAGS_SHOUT;
	if(oflags & DP_MSG_CHAT_OFLAGS_ACTION) {
		chat.appendAction(d->usernameSpan(sender), safetext);
	} else if(d->compactMode) {
		chat.appendMessageCompact(sender, d->usernameSpan(sender), safetext, isValidShout, isValidAlert);
	} else {
		chat.appendMessage(sender, d->usernameSpan(sender), safetext, isValidShout, isValidAlert);
	}

	if(isValidAlert) {
		for(int i=0;i<d->tabs->count();++i) {
			if(d->tabs->tabData(i).toInt() == chatId) {
				d->tabs->setCurrentIndex(i);
				break;
			}
		}
		emit expandRequested();
	} else if(chatId != d->currentChat) {
		for(int i=0;i<d->tabs->count();++i) {
			if(d->tabs->tabData(i).toInt() == chatId) {
				d->tabs->setTabTextColor(i, QColor(218, 68, 83));
				break;
			}
		}
	}

	if(!d->myline->hasFocus() || chatId != d->currentChat || isValidAlert)
		notification::playSound(notification::Event::CHAT);

	if(wasAtEnd || isValidAlert) {
		d->scrollChatToEnd(chatId);
	}
}

void ChatWidget::setPinnedMessage(const QString &message)
{
	const bool wasAtEnd = d->isAtEnd();
	d->pinned->setPinText(message);
	if(wasAtEnd) {
		d->scrollToEnd();
	}
}

void ChatWidget::systemMessage(const QString& message, bool alert)
{
	const bool wasAtEnd = d->isAtEnd();
	const QString safetext = htmlutils::linkify(message.toHtmlEscaped());
	if(alert) {
		d->publicChat().appendMessageCompact(0, QString(), safetext, false, true);
		emit expandRequested();
	} else {
		d->publicChat().appendNotification(safetext);
	}

	if(wasAtEnd || alert) {
		d->scrollChatToEnd(0);
	}
}

void ChatWidget::scrollBarMoved(int)
{
	d->wasAtEnd = d->isAtEnd();
}

void ChatWidget::sendMessage(QString chatMessage)
{
	DP_PERF_SCOPE("send_message");
	static QRegularExpression rtrimRegex{QStringLiteral("\\s+\\z")};
	chatMessage.replace(rtrimRegex, QString{});

	uint8_t tflags = d->preserveChat ? 0 : DP_MSG_CHAT_TFLAGS_BYPASS;
	uint8_t oflags = 0;
	QString effectiveMessage = chatMessage;

	if(chatMessage.at(0) == '/') {
		// Special commands

		int split = chatMessage.indexOf(' ');
		if(split<0)
			split = chatMessage.length();

		const auto cmd = chatMessage.mid(1, split-1);
		const auto params = chatMessage.mid(split).trimmed();

		if(cmd == QStringLiteral("clear")) {
			clear();
			return;

		} else if(cmd.at(0)=='!' && d->currentChat == 0) {
			if(!d->userlist || !d->userlist->isOperator(d->myId)) {
				systemMessage(tr("/!: only operators are allowed to send shouts."));
				return;
			} else if (d->currentChat != 0) {
				systemMessage(tr("/!: can only shout in a public chat."));
				return;
			} else if(chatMessage.length() <= 2) {
				systemMessage(tr("/!: no text given."));
				return;
			} else {
				effectiveMessage = chatMessage.mid(2);
				oflags = DP_MSG_CHAT_OFLAGS_SHOUT;
			}

		} else if(cmd == QStringLiteral("alert")) {
			if(!d->userlist || !d->userlist->isOperator(d->myId)) {
				systemMessage(tr("/alert: only operators are allowed to send alerts."));
				return;
			} else if(params.isEmpty()) {
				systemMessage(tr("/alert: no text given."));
				return;
			} else {
				effectiveMessage = params;
				oflags = DP_MSG_CHAT_OFLAGS_ALERT;
			}

		} else if(cmd == QStringLiteral("me")) {
			if(params.isEmpty()) {
				systemMessage(tr("/me: no text given."));
				return;
			} else {
				oflags = DP_MSG_CHAT_OFLAGS_ACTION;
				effectiveMessage = params;
			}

		} else if(cmd == QStringLiteral("pin")) {
			if(!d->userlist || !d->userlist->isOperator(d->myId)) {
				systemMessage(tr("/pin: only operators are allowed to pin."));
				return;
			} else if (d->currentChat != 0) {
				systemMessage(tr("/pin: can only pin in a public chat."));
				return;
			} else if(params.isEmpty()) {
				systemMessage(tr("/pin: no text given."));
				return;
			} else {
				oflags = DP_MSG_CHAT_OFLAGS_PIN | DP_MSG_CHAT_OFLAGS_SHOUT;
				effectiveMessage = params;
			}

		} else if(cmd == QStringLiteral("unpin")) {
			if(!d->userlist || !d->userlist->isOperator(d->myId)) {
				systemMessage(tr("/unpin: only operators are allowed to unpin."));
				return;
			} else if (d->currentChat != 0) {
				systemMessage(tr("/unpin: can only unpin in a public chat."));
				return;
			} else {
				oflags = DP_MSG_CHAT_OFLAGS_PIN | DP_MSG_CHAT_OFLAGS_SHOUT;
				effectiveMessage = QStringLiteral("-");
			}

		} else if(cmd == QStringLiteral("roll")) {
			// TODO this should be done serverside to prevent cheating
			utils::DiceRoll result = utils::diceRoll(params.isEmpty() ? QStringLiteral("1d6") : params);
			if(result.number>0) {
				oflags = DP_MSG_CHAT_OFLAGS_ACTION;
				effectiveMessage = "rolls " + result.toString();
			} else {
				systemMessage(tr("/roll: invalid dice roll description."));
				return;
			}

		} else if(cmd == QStringLiteral("help")) {
			//: Don't translate the commands, only their descriptions!
			const QString text = tr(
				"Available client commands:\n"
				"/help - show this message\n"
				"/clear - clear chat window\n"
				"/! <text> - make an announcement (Operators only)\n"
				"/alert <text> - send a high priority alert (Operators only)\n"
				"/pin <text> - pin a message to the top of the chat box (Operators only)\n"
				"/unpin - remove pinned message (Operators only)\n"
				"/me <text> - send action-type message\n"
				"/roll <dice> - roll dice, e.g. 1d6 for a six-sided die"
			);
			systemMessage(text);
			return;

		} else {
			systemMessage(tr("Unknown command: %1").arg(chatMessage));
			return;
		}
	}

	// Send the chat message
	int target = d->currentChat;
	uint8_t contextId = d->myId;
	drawdance::Message msg = target == 0
		? drawdance::Message::makeChat(contextId, tflags, oflags, effectiveMessage)
		: drawdance::Message::makePrivateChat(contextId, target, oflags, effectiveMessage);
	emit message(msg);
}

void ChatWidget::chatTabSelected(int index)
{
	d->chats[d->currentChat].scrollPosition = d->view->verticalScrollBar()->value();

	const int id = d->tabs->tabData(index).toInt();
	Q_ASSERT(d->chats.contains(id));
	d->view->setDocument(d->chats[id].doc);
	d->view->verticalScrollBar()->setValue(d->chats[id].scrollPosition);
	d->tabs->setTabTextColor(index, QColor());
	d->currentChat = d->tabs->tabData(index).toInt();
	d->updatePreserveModeUi();
}

void ChatWidget::chatTabClosed(int index)
{
	const int id = d->tabs->tabData(index).toInt();
	Q_ASSERT(d->chats.contains(id));
	if(id == 0) {
		// Can't close the public chat
		return;
	}

	d->tabs->removeTab(index);

	delete d->chats[id].doc;
	d->chats.remove(id);
}

void ChatWidget::showChatContextMenu(const QPoint &pos)
{
	contextMenuAboutToShow();
	QMenu *menu = d->view->createStandardContextMenu();
	menu->addSeparator();

	QAction *actions[] = {
		d->clearAction, d->compactAction, d->attachAction, d->detachAction};
	for(QAction *action : actions) {
		if(action && action->isVisible()) {
			menu->addAction(action);
		}
	}

	menu->exec(d->view->mapToGlobal(pos));
	delete menu;
}

void ChatWidget::contextMenuAboutToShow()
{
	if(ALLOW_DETACH) {
		d->attachAction->setVisible(!d->isAttached);
		d->detachAction->setVisible(d->isAttached);
	}
}

void ChatWidget::setCompactMode(bool compact)
{
	if(!COMPACT_ONLY) {
		dpApp().settings().setCompactChat(compact);
	}
}

void ChatWidget::attach()
{
	QWidget *win = parentWidget();
	while(win->parent() != nullptr) {
		win = win->parentWidget();
	}
	win->close();
}

void ChatWidget::resizeEvent(QResizeEvent *)
{
	if(d->wasAtEnd) {
		d->scrollToEnd();
	}
}

}
