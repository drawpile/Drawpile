#ifndef THINSRV_EXTBANS_H
#define THINSRV_EXTBANS_H
#include "libserver/serverconfig.h"
#include <QDateTime>
#include <QHostAddress>
#include <QObject>
#include <QUrl>
#include <climits>
#include <functional>

class QJsonObject;
class QNetworkReply;
class QTimer;

namespace server {

class ConfigKey;
class ServerConfig;
class ServerLog;

class ExtBans : public QObject {
	Q_OBJECT
public:
	enum class RefreshResult { Ok, AlreadyInProgress, NotActive };

	explicit ExtBans(ServerConfig *config, QObject *parent = nullptr);

	QJsonObject status() const;

	RefreshResult refreshNow();

	void loadFromCache();

public slots:
	void start();
	void stop();

signals:
	void deactivated();

private slots:
	void handleConfigValueChange(const ConfigKey &key);
	void refresh();

private:
	static constexpr int MIN_INTERVAL = 60;
	static constexpr int MAX_INTERVAL = INT_MAX / 1000;

	static const QString &userAgent();

	void handleReply(
		QNetworkReply *reply, const QString &cacheUrl, const QString &cacheKey);

	QVector<ExtBan> handleBans(const QJsonObject &object);

	static bool
	handleIps(const QJsonArray &array, bool exclude, QVector<BanIpRange> &outRanges);

	static QVector<BanSystemIdentifier> handleSystem(const QJsonArray &array);

	static QVector<BanUser> handleUsers(const QJsonArray &array);

	ServerLog *logger();

	void setUrl(const QString &url);
	void setIntervalSecs(int intervalSecs);
	void update();

	ServerConfig *m_config;
	bool m_started = false;
	bool m_active = false;
	QUrl m_url;
	int m_intervalMsecs = 0;
	QTimer *m_refreshTimer;
};

}

#endif
