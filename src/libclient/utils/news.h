// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBCLIENT_UTILS_NEWS_H
#define LIBCLIENT_UTILS_NEWS_H

#include <QDate>
#include <QObject>
#include <QUrl>
#include <QVector>

class QJsonDocument;
class QNetworkReply;

namespace utils {

class StateDatabase;

class News final : public QObject {
	Q_OBJECT
public:
	struct Version {
		int server;
		int major;
		int minor;
		int beta;

		static Version parse(const QString &s);
		static Version invalid() { return {0, 0, 0, 0}; }

		bool isValid() const { return server > 0; }
		bool isBeta() const { return beta > 0; }
		bool isNewerThan(const Version &other) const;
		QString toString() const;
	};

	struct Update {
		Version version;
		QDate date;
		QUrl url;

		static Update invalid()
		{
			return {Version::invalid(), QDate{}, QUrl{}};
		}

		bool isValid() const { return version.isValid(); }
	};

	explicit News(utils::StateDatabase &state, QObject *parent = nullptr);
	~News() override;

	void check();
	void forceCheck(int delayMsec);
	void checkExisting();
	QDate lastCheck() const;

signals:
	void newsAvailable(const QString &content);
	void updateAvailable(const Update &update);
	void fetchInProgress(bool inProgress);

private:
	static constexpr long long CHECK_STALE_DAYS = 1;
	static constexpr long long CHECK_EXISTING_STALE_DAYS = 3;
	static constexpr char URL[] =
		"https://drawpile.net/files/metadata/news.json";
	static constexpr char FALLBACK_UPDATE_URL[] = "https://drawpile.net/";

	struct Article {
		QString content;
		QDate date;
	};

	void doCheck(bool force);
	void fetch(const QDate &date, bool showErrorAsNews);
	void fetchFinished(QDate date, QNetworkReply *reply, bool showErrorAsNews);
	QVector<Article> parseNews(const QJsonDocument &doc);
	QVector<Update> parseUpdates(const QJsonDocument &doc);
	void checkUpdateAvailable();

	void showMessageAsNews(const QString &message);

	class Private;
	Private *d;
};

}

#endif
