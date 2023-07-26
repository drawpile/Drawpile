// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/startdialog/join.h"
#include "desktop/main.h"
#include "desktop/utils/sanerformlayout.h"
#include "desktop/widgets/recentscroll.h"
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
		tr("Enter a <strong>session URL</strong>, <strong>host name</strong>, "
		   "<strong>IP address</strong> or <strong>room code</strong>:")};
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
}

void Join::activate()
{
	emit showButtons();
	updateJoinButton();
}

void Join::accept()
{
	QUrl url = getUrl();
	if(url.isValid()) {
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
	if(looksLikeRoomcode(address)) {
		m_addressEdit->setText(QString{});
		m_addressEdit->setPlaceholderText(tr("Searching…"));
		m_addressEdit->setReadOnly(true);
		resolveRoomcode(address, getRoomcodeServerUrls());
	} else {
		QString fixedUpUrl = fixUpInviteAddress(address);
		if(!fixedUpUrl.isEmpty()) {
			QSignalBlocker blocker{m_addressEdit};
			m_addressEdit->setText(fixedUpUrl);
		}
	}
	updateJoinButton();
}

void Join::resetAddressPlaceholderText()
{
	m_addressEdit->setPlaceholderText(QStringLiteral("drawpile://…"));
}

void Join::updateJoinButton()
{
	emit enableJoin(getUrl().isValid());
}

bool Join::looksLikeRoomcode(const QString &address)
{
	static QRegularExpression roomcodeRe{"\\A[A-Z]{5}\\z"};
	return roomcodeRe.match(address).hasMatch();
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

QStringList Join::getRoomcodeServerUrls() const
{
	QStringList servers;
	for(const sessionlisting::ListServer &ls :
		sessionlisting::ListServerModel::listServers(
			dpApp().settings().listServers(), true)) {
		if(ls.privateListings) {
			servers.append((ls.url));
		}
	}
	return servers;
}

void Join::resolveRoomcode(const QString &roomcode, const QStringList &servers)
{
	if(servers.isEmpty()) {
		// Tried all the servers and didn't find the code
		finishResolvingRoomcode(QString{});
		m_addressMessageLabel->setText(tr("Roomcode not found!"));
	} else {
		QUrl listServer = servers.first();
		sessionlisting::AnnouncementApiResponse *response =
			sessionlisting::queryRoomcode(listServer, roomcode);
		connect(
			response, &sessionlisting::AnnouncementApiResponse::finished, this,
			[this, roomcode, servers](
				const QVariant &result, const QString &, const QString &error) {
				if(error.isEmpty()) {
					sessionlisting::Session session =
						result.value<sessionlisting::Session>();
					QString portSuffix =
						session.port == cmake_config::proto::port()
							? QString{}
							: QStringLiteral(":%1").arg(session.port);
					QString address =
						QStringLiteral("%1%2%3/%4")
							.arg(SCHEME, session.host, portSuffix, session.id);
					finishResolvingRoomcode(address);
				} else {
					// Not found, try the next list server.
					resolveRoomcode(roomcode, servers.mid(1));
				}
			});
		connect(
			response, &sessionlisting::AnnouncementApiResponse::finished,
			response, &QObject::deleteLater);
	}
}

void Join::finishResolvingRoomcode(const QString &address)
{
	resetAddressPlaceholderText();
	m_addressEdit->setReadOnly(false);
	m_addressEdit->setText(address);
	m_addressEdit->setFocus();
}

QUrl Join::getUrl() const
{
	QString address = m_addressEdit->text().trimmed();
	if(!address.startsWith(SCHEME)) {
		address.prepend(SCHEME);
	}
	QUrl url = QUrl(address, QUrl::TolerantMode);
	return url.isValid() || url.host().isEmpty() ? url : QUrl{};
}

}
}
