// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/join.h"
#include "cmake-config/config.h"
#include "desktop/main.h"
#include "desktop/widgets/recentscroll.h"
#include "libclient/config/config.h"
#include "libshared/net/netutils.h"
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
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

	CFG_BIND_SET(
		dpAppConfig(), LastJoinAddress, m_addressEdit, QLineEdit::setText);
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
		dpAppConfig()->setLastJoinAddress(m_addressEdit->text().trimmed());
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
	QString fixedUpUrl = fixUpInviteOrWebAddress(address);
	if(!fixedUpUrl.isEmpty()) {
		QSignalBlocker blocker{m_addressEdit};
		m_addressEdit->setText(fixedUpUrl);
	}
	updateJoinButton();
}

void Join::resetAddressPlaceholderText()
{
	m_addressEdit->setPlaceholderText(
		QStringLiteral("drawpile://… or wss://…"));
}

void Join::updateJoinButton()
{
	emit enableJoin(getUrl().isValid());
}

QString Join::fixUpInviteOrWebAddress(const QString &address)
{
	static QRegularExpression inviteRe{
		"\\A/invites/([^:/]+)(?::([0-9]+))?/+([a-zA-Z0-9:-]{1,50})/*\\z"};

	QUrl url = QUrl::fromUserInput(address);
	if(!url.scheme().startsWith("http", Qt::CaseInsensitive)) {
		return QString();
	}

	QRegularExpressionMatch match = inviteRe.match(url.path());
	if(match.hasMatch()) {
		url.setScheme(QStringLiteral("drawpile"));
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
			QUrlQuery query(url);
			query.removeAllQueryItems(QStringLiteral("p"));
			query.addQueryItem(QStringLiteral("p"), password);
			url.setQuery(query);
			url.setFragment(QString());
		}

		return url.toString();
	}

	QUrlQuery query(url);
	QString host = query.queryItemValue(QStringLiteral("host"));
	if(!host.isEmpty()) {
		url.setScheme(QStringLiteral("drawpile"));
		url.setHost(host);
		query.removeAllQueryItems(QStringLiteral("host"));

		QString password = url.fragment();
		if(!password.isEmpty()) {
			query.removeAllQueryItems(QStringLiteral("p"));
			query.addQueryItem(QStringLiteral("p"), password);
			url.setFragment(QString());
		}

		url.setQuery(query);
		return url.toString();
	}

	return QString();
}

QUrl Join::getUrl() const
{
	QString address = m_addressEdit->text().trimmed();
	if(!address.isEmpty()) {
		QUrl url = QUrl(
			net::addSchemeToUserSuppliedAddress(address), QUrl::TolerantMode);
		if(url.isValid() && !url.host().isEmpty()) {
			return url;
		}
	}
	return QUrl();
}

}
}
