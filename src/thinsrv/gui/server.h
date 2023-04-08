// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef GUI_SERVER_CONNECTOR_H
#define GUI_SERVER_CONNECTOR_H

#include <QObject>
#include <QJsonArray>

#include "libserver/jsonapi.h"

namespace server {
namespace gui {

class SessionListModel;

/**
 * @brief Abstract base class for server connectors
 */
class Server : public QObject
{
	Q_OBJECT
public:
	explicit Server(QObject *parent=nullptr);

	/**
	 * @brief Is this the local server?
	 */
	virtual bool isLocal() const = 0;

	/**
	 * @brief Get the server's address
	 *
	 * For local servers, this is the either the manually specified announcement
	 * address or a buest guess.
	 */
	virtual QString address() const = 0;

	/**
	 * @brief Get the port the server is (or will be) running on
	 */
	virtual int port() const = 0;

	/**
	 * @brief Make a request to the server's JSON API
	 *
	 * An apiResponse signal with the given requestId will be emitted
	 * when the server responds.
	 *
	 * @param requestId internal request ID
	 * @param method request method
	 * @param path path
	 * @param request request body
	 */
	virtual void makeApiRequest(const QString &requestId, JsonApiMethod method, const QStringList &path, const QJsonObject &request) = 0;

	/**
	 * @brief Refresh the session list
	 */
	void refreshSessionList();

	//! Get the last refreshed session list
	SessionListModel *sessionList() const { return m_sessions; }

signals:
	void apiResponse(const QString &requestId, const JsonApiResult &result);
	void sessionListRefreshed(const QJsonArray &sessionlist);

protected slots:
	void onApiResponse(const QString &requestId, const JsonApiResult &result);

private:
	SessionListModel *m_sessions;
};

}
}

#endif
