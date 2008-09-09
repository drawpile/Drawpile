#ifndef DP_SERVER_H
#define DP_SERVER_H

#include <QObject>
#include <QHostAddress>
#include <QHash>

class QTcpServer;

#include "board.h"

namespace server {

class Client;

/**
 * The drawpile server.
 */
class Server : public QObject {
	Q_OBJECT
	static const int MAXCLIENTS = 255;
	public:
		enum State {
			// Business as usual, relay packets.
			NORMAL,
			// Synchronize new users
			SYNC
		};

		Server(QObject *parent=0);

		/**
		 * \brief Start the server.
		 * @param port listening port
		 * @param address listening address
		 */
		void start(quint16 port, const QHostAddress& address = QHostAddress::Any);
		/**
		 * \brief Stop the server. All clients are disconnected.
		 */
		void stop();

		/**
		 * \brief Check if a user with the specified ID exists.
		 * @return true if user is connected.
		 */
		bool hasClient(int id) { return _clients.contains(id); }

		/**
		 * \brief Check if the user with the specified name is logged in.
		 * @return true if named user is logged in
		 */
		bool hasClient(const QString& name);

		/**
		 * \brief Return the number of clients
		 */
		int clients() const { return _clients.size(); }

		/**
		 * \brief Synchronize users so new people can join.
		 */
		void syncUsers();

		/**
		 * Get a new client up to speed by informing it about all other
		 * clients.
		 */
		void briefClient(int id);

		/**
		 * \brief Redistribute data to users.
		 * @param sync distribute syncing users
		 * @param active distribute to active users
		 * @return number of users to whom data was sent
		 */
		int redistribute(bool sync, bool active, const QByteArray& data);

		/**
		 * \brief Get the drawing board used
		 */
		Board& board() { return _board; }
		const Board& board() const { return _board; }

	private slots:
		// A new client was added
		void newClient();
		// A client was removed
		void killClient(int id);
		// A client's lock state changed
		void updateLock(int id, bool state);

	private:
		// Request raster data from some user
		void requestRaster();

		//TODO disable copy constructors
		QTcpServer *_server;
		QHash<int,Client*> _clients;
		int _lastclient;

		State _state;
		Board _board;
};

}

#endif

