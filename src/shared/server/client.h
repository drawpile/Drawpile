#ifndef DP_SRV_CLIENT_H
#define DP_SRV_CLIENT_H

#include <QObject>
#include <QByteArray>

class QTcpSocket;

namespace protocol {
	class MessageQueue;
	class LoginId;
	class Message;
	class Packet;
	class BinaryChunk;
}

namespace server {

class Server;

/**
 * A client of the server.
 */
class Client : public QObject {
	Q_OBJECT
	public:
		enum State {
			// Expect protocol identifier
			CONNECT,
			// Expect password
			AUTHENTICATION,
			// Expect user details
			LOGIN,
			// Synchronizing
			SYNC,
			// User is an active part of the session
			ACTIVE
		};
		/**
		 * Construct a new client. Clients start out in CONNECT
		 * state.
		 */
		Client(int id, Server *parent, QTcpSocket *socket);

		/**
		 * Kick the user out. A message is sent to the user and the connection is cut.
		 * @param message message sent to the user to indicate the reason why they were kicked out.
		 */
		void kick(const QString& message);

		/**
		 * Get the state of the user.
		 */
		State state() const { return _state; }

		/**
		 * Get the ID of the user
		 */
		int id() const { return _id; }

		/**
		 * Get the name of the user. The name is only valid after LOGIN
		 * state.
		 */
		const QString& name() const { return _name; }

		/**
		 * Set the name of the user
		 */
		void setName(const QString& name) { _name = name; }

		/**
		 * Is the user locked? Drawing commands sent by a locked user
		 * are dropped.
		 */
		bool isLocked() const { return _lock | _synclock; }

		/**
		 * Send arbitrary data to this user.
		 */
		void send(const QByteArray& data);

		/**
		 * Lock the user for synchronization. The users will lock themselves
		 * as soon as they are ready, so after issuing the locks we need
		 * to wait for them to take effect.
		 */
		void syncLock();

		/**
		 * Remove the synchronization lock.
		 */
		void syncUnlock();

		/**
		 * Request the user to set aside a copy of their board and start
		 * sending it here.
		 */
		void requestRaster();

		/**
		 * Return a info message detailing this user.
		 */
		QString toMessage() const;

	signals:
		void disconnected(int id);

		/* User was locked or unlocked */
		void lock(int id, bool state);

	private slots:
		// New data is available
		void newData();
		// Kick the user out for a protocol violation
		void bail(const char* message="unspecified error");
		// Socket was closed
		void closeSocket();

	private:
		void handleLogin(const protocol::LoginId *pkt);
		void handleMessage(const protocol::Message *msg);
		void handleBinary(const protocol::BinaryChunk *bin);
		void handleDrawing(const protocol::Packet *bin);

		void loginLocalUser(const QStringList& tokens);

		int _id;
		QString _name;

		Server *_server;
		protocol::MessageQueue *_socket;

		State _state;
		bool _lock;
		bool _synclock;
		bool _giveraster;
};

}

#endif

