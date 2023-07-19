// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBCLIENT_UTILS_NEWS_H
#define LIBCLIENT_UTILS_NEWS_H

#include <QDate>
#include <QObject>
#include <QVector>

class QByteArray;
class QNetworkReply;

namespace utils {

class News final : public QObject {
	Q_OBJECT
public:
	explicit News(QObject *parent = nullptr);
	~News() override;

	void check();
	void forceCheck(int delayMsec);
	void checkExisting();
	QDate lastCheck() const;

signals:
	void newsAvailable(const QString &content);
	void fetchInProgress(bool inProgress);

private:
	static constexpr long long CHECK_STALE_DAYS = 1;
	static constexpr long long CHECK_EXISTING_STALE_DAYS = 3;
	static constexpr char URL[] =
		"https://drawpile.net/files/metadata/news.json";

	struct Article {
		QString content;
		QDate date;
	};

	void doCheck(bool force);
	void fetch(const QDate &date, bool showErrorAsNews);
	void fetchFinished(QDate date, QNetworkReply *reply, bool showErrorAsNews);
	QVector<Article> parse(const QByteArray &bytes);

	void showMessageAsNews(const QString &message);

	class Private;
	Private *d;
};

}

#endif
