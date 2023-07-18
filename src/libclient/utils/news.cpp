// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/news.h"
#include "libclient/utils/database.h"
#include "libshared/util/networkaccess.h"
#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <functional>


Q_LOGGING_CATEGORY(lcDpNews, "net.drawpile.news", QtWarningMsg)

namespace utils {

class News::Private {
public:
	Private()
		: m_db{utils::db::sqlite(
			  QStringLiteral("drawpile_state_connection"),
			  QStringLiteral("state"), QStringLiteral("state.db"))}
	{
		initDb();
	}

	bool fetching() const { return m_fetching; }
	void setFetching(bool fetching) { m_fetching = fetching; }

	QString readNewsContentFor(const QDate &date)
	{
		QSqlQuery query(m_db);
		bool hasRow = utils::db::exec(
						  query,
						  "select content from news where date <= ?\n"
						  "order by date desc limit 1",
						  {date.toString(Qt::ISODate)}) &&
					  query.next();
		return hasRow ? query.value(0).toString() : QString{};
	}

	bool shouldFetch(const QDate &date)
	{
		QDate lastCheck = readLastCheck();
		long long dayDiff = lastCheck.isValid() ? lastCheck.daysTo(date) : -999;
		qCDebug(lcDpNews, "Day delta %lld", dayDiff);
		return dayDiff != 0;
	}

	bool writeNewsAt(const QVector<News::Article> articles, const QDate &today)
	{
		return utils::db::tx(m_db, [&] {
			QSqlQuery query{m_db};
			if(!utils::db::exec(query, QStringLiteral("delete from news"))) {
				return false;
			}

			QString sql = QStringLiteral(
				"insert into news (content, date) values (?, ?)");
			if(!utils::db::prepare(query, sql)) {
				return false;
			}

			for(const News::Article &article : articles) {
				query.bindValue(0, article.content);
				query.bindValue(
					1, article.date.isValid()
						   ? article.date.toString(Qt::ISODate)
						   : QStringLiteral("0000-00-00"));
				if(!utils::db::execPrepared(query, sql)) {
					return false;
				}
			}

			return putStateOn(
				query, LAST_CHECK_KEY, today.toString(Qt::ISODate));
		});
	}

	QDate readLastCheck() const
	{
		return QDate::fromString(
			getStateOn(LAST_CHECK_KEY).toString(), Qt::ISODate);
	}

private:
	static constexpr char LAST_CHECK_KEY[] = "news:lastcheck";

	void initDb()
	{
		QSqlQuery query{m_db};
		utils::db::exec(query, "pragma foreign_keys = on");
		utils::db::exec(
			query, "create table if not exists migrations (\n"
				   "	migration_id integer primary key not null)");
		utils::db::exec(
			query, "create table if not exists state (\n"
				   "	key text primary key not null,\n"
				   "	value)");
		utils::db::exec(
			query, "create table if not exists news (\n"
				   "	content text not null,\n"
				   "	date text not null)");
	}

	bool putStateOn(QSqlQuery &query, const QString &key, const QVariant &value)
	{
		QString sql =
			QStringLiteral("insert into state (key, value) values (?, ?)\n"
						   "	on conflict (key) do update set value = ?");
		return utils::db::exec(query, sql, {key, value, value});
	}

	QVariant getStateOn(const QString &key) const
	{
		QSqlQuery query(m_db);
		return getStateWith(query, key);
	}

	QVariant getStateWith(QSqlQuery &query, const QString &key) const
	{
		bool hasRow =
			utils::db::exec(
				query, "select value from state where key = ?", {key}) &&
			query.next();
		return hasRow ? query.value(0) : QVariant{};
	}

	QSqlDatabase m_db;
	bool m_fetching = false;
};

News::News(QObject *parent)
	: QObject{parent}
	, d{new Private}
{
}

News::~News()
{
	delete d;
}

void News::check()
{
	emit fetchInProgress(true);
	doCheck(false);
}

void News::forceCheck(int delayMsec)
{
	emit fetchInProgress(true);
	showMessageAsNews(tr("Checking for updates…"));
	QTimer::singleShot(delayMsec, this, std::bind(&News::doCheck, this, true));
}

QDate News::lastCheck() const
{
	return d->readLastCheck();
}

void News::doCheck(bool force)
{
	QDate date = QDate::currentDate();
	if(force) {
		qCDebug(lcDpNews, "Force fetching news");
		fetch(date, true);
	} else {
		QString content = d->readNewsContentFor(date);
		if(content.isEmpty()) {
			qCDebug(lcDpNews, "Fetching initial news");
			if(!force) {
				showMessageAsNews(tr("Checking for updates…"));
			}
			fetch(date, true);
		} else {
			emit newsAvailable(content);
			if(d->shouldFetch(date)) {
				qCDebug(lcDpNews, "Fetching news");
				fetch(date, false);
			} else {
				qCDebug(lcDpNews, "Not fetching news");
				emit fetchInProgress(false);
			}
		}
	}
}

void News::fetch(const QDate &date, bool showErrorAsNews)
{
	d->setFetching(true);
	QNetworkRequest req(QUrl{URL});
	QNetworkReply *reply = networkaccess::getInstance()->get(req);
	connect(reply, &QNetworkReply::finished, this, [=] {
		fetchFinished(date, reply, showErrorAsNews);
		qCDebug(lcDpNews, "Done fetching news");
		d->setFetching(false);
		emit fetchInProgress(false);
	});
	connect(this, &QObject::destroyed, reply, &QObject::deleteLater);
}

void News::fetchFinished(QDate date, QNetworkReply *reply, bool showErrorAsNews)
{
	if(reply->error() != QNetworkReply::NoError) {
		qCWarning(
			lcDpNews, "Network error fetching news: %s",
			qUtf8Printable(reply->errorString()));
		if(showErrorAsNews) {
			showMessageAsNews(
				tr("Network error: %1").arg(reply->errorString()));
		}
		return;
	}

	QVector<News::Article> articles = parse(reply->readAll());
	if(articles.isEmpty()) {
		if(showErrorAsNews) {
			showMessageAsNews(tr("Couldn't make sense of fetched news."));
		}
		return;
	}

	if(!d->writeNewsAt(articles, date)) {
		if(showErrorAsNews) {
			showMessageAsNews(tr("Couldn't save news."));
		}
		return;
	}

	QString content = d->readNewsContentFor(date);
	if(content.isEmpty()) {
		if(showErrorAsNews) {
			showMessageAsNews(tr("No news available."));
		}
		return;
	}

	emit newsAvailable(content);
}

QVector<News::Article> News::parse(const QByteArray &bytes)
{
	QJsonParseError err;
	QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
	if(doc.isNull() && err.error != QJsonParseError::NoError) {
		qCWarning(
			lcDpNews, "Error parsing news: %s",
			qUtf8Printable(err.errorString()));
		return {};
	}

	QJsonValue news = doc["news"];
	if(!news.isArray()) {
		qCWarning(lcDpNews, "News element is not an array");
		return {};
	}

	QVector<Article> articles;
	for(QJsonValue value : news.toArray()) {
		QString content = value["content"].toString();
		if(content.isEmpty()) {
			qCWarning(lcDpNews, "News article content is empty");
			continue;
		}

		QString dateString = value["date"].toString();
		QDate date;
		if(!dateString.isEmpty()) {
			date = QDate::fromString(dateString, Qt::ISODate);
			if(!date.isValid()) {
				qCWarning(lcDpNews, "News article date is invalid");
				continue;
			}
		}

		articles.append({content, date});
	}

	if(articles.isEmpty()) {
		qCWarning(lcDpNews, "No news articles in response");
	}
	return articles;
}

void News::showMessageAsNews(const QString &message)
{
	emit newsAvailable(
		QStringLiteral("<p>%1</p>").arg(message.toHtmlEscaped()));
}

}
