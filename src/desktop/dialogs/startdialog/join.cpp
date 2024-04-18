// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/join.h"
#include "desktop/main.h"
#include "desktop/widgets/recentscroll.h"
#include "libclient/net/server.h"
#include "libclient/utils/listservermodel.h"
#include "libshared/listings/announcementapi.h"
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>
#include <QSpacerItem>
#include <QUrlQuery>
#include <QVBoxLayout>

namespace dialogs {
namespace startdialog {

Join::Join(QWidget *parent)
	: Page{parent}
{
	QVBoxLayout *layout = new QVBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);

	QLabel *urlLabel = new QLabel{
		tr("Enter a <strong>session URL</strong>, <strong>host name</strong> "
		   "or <strong>IP address</strong>:")};
	layout->addWidget(urlLabel);

	m_addressEdit = new QLineEdit;
	resetAddressPlaceholderText();
	connect(
		m_addressEdit, &QLineEdit::textChanged, this, &Join::addressChanged);

	layout->addWidget(m_addressEdit);

	m_addressMessageLabel = new QLabel;
	m_addressMessageLabel->setWordWrap(true);
	layout->addWidget(m_addressMessageLabel);

	QLabel *recentLabel = new QLabel{tr("Recent:")};
	layout->addWidget(recentLabel);

	m_recentScroll =
		new widgets::RecentScroll{widgets::RecentScroll::Mode::Join};
	layout->addWidget(m_recentScroll);
	connect(
		m_recentScroll, &widgets::RecentScroll::clicked, this,
		&Join::setAddress);
	connect(
		m_recentScroll, &widgets::RecentScroll::doubleClicked, this,
		&Join::acceptAddress);

	dpApp().settings().bindLastJoinAddress(m_addressEdit, &QLineEdit::setText);
}

void Join::activate()
{
	emit showButtons();
	updateJoinButton();
	m_addressEdit->setFocus();
	m_addressEdit->selectAll();
}

void Join::accept()
{
	QUrl url = getUrl();
	if(url.isValid()) {
		dpApp().settings().setLastJoinAddress(m_addressEdit->text().trimmed());
		emit join(url);
	}
}

void Join::setAddress(const QString &address)
{
	m_addressEdit->setText(address);
}

void Join::acceptAddress(const QString &address)
{
	setAddress(address);
	accept();
}

void Join::addressChanged(const QString &address)
{
	m_addressMessageLabel->setText(QString{});
	QString fixedUpUrl = fixUpInviteAddress(address);
	if(!fixedUpUrl.isEmpty()) {
		QSignalBlocker blocker{m_addressEdit};
		m_addressEdit->setText(fixedUpUrl);
	}
	updateJoinButton();
}

void Join::resetAddressPlaceholderText()
{
	m_addressEdit->setPlaceholderText(
#if defined(HAVE_TCPSOCKETS) && defined(HAVE_WEBSOCKETS)
		QStringLiteral("drawpile://… or wss://…")
#elif defined(HAVE_TCPSOCKETS)
		QStringLiteral("drawpile://…")
#elif defined(HAVE_WEBSOCKETS)
		QStringLiteral("wss://…")
#else
		QStringLiteral("???://…")
#endif
	);
}

void Join::updateJoinButton()
{
	emit enableJoin(getUrl().isValid());
}

QString Join::fixUpInviteAddress(const QString &address)
{
	static QRegularExpression inviteRe{
		"\\A/invites/([^:/]+)(?::([0-9]+))?/+([a-zA-Z0-9-]{1,50})/*\\z"};

	QUrl url = QUrl::fromUserInput(address);
	if(!url.scheme().startsWith("http", Qt::CaseInsensitive)) {
		return QString{};
	}

	QRegularExpressionMatch match = inviteRe.match(url.path());
	if(!match.hasMatch()) {
		return QString{};
	}

	url.setScheme("drawpile");
	url.setHost(match.captured(1));
	int port = match.captured(2).toInt();
	if(port > 0 && port <= 65535 && port != cmake_config::proto::port()) {
		url.setPort(port);
	} else {
		url.setPort(-1);
	}
	url.setPath(QStringLiteral("/%1").arg(match.captured(3)));

	QString password = url.fragment();
	if(!password.isEmpty()) {
		QUrlQuery query{url};
		query.addQueryItem(QStringLiteral("p"), password);
		url.setQuery(query);
		url.setFragment(QString{});
	}

	return url.toString();
}

QUrl Join::getUrl() const
{
	QString address = m_addressEdit->text().trimmed();
	QUrl url = QUrl(
		net::Server::addSchemeToUserSuppliedAddress(address),
		QUrl::TolerantMode);
	return url.isValid() || url.host().isEmpty() ? url : QUrl{};
}

}
}
