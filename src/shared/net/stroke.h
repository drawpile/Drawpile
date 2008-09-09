#ifndef DP_PROTO_STROKE_H
#define DP_PROTO_STROKE_H

#include <QVarLengthArray>

#include "protocol.h"

namespace protocol {

/**
 * A stroke coordinate.
 */
struct XYZ {
	XYZ() { }
	XYZ(int X, int Y, int Z) : x(X), y(Y), z(Z) { }
	int x, y, z;
};

/**
 * A point in the stroke path.
 * May store more than one point, to allow the data to be sent
 * more efficiently and atomically.
 */
class StrokePoint : public Packet {
	public:
		/**
		 * Construct a stroke point message. More points can bee added
		 * with addPoint()
		 * @param user user ID. A client must set this to their ID when sending a message.
		 * @param x stroke X coordinate
		 * @param y stroke Y coordinate
		 * @param z stroke pressure
		 */
		StrokePoint(int user, unsigned int x, unsigned int y, unsigned int z)
			: Packet(STROKE), _user(user) {
				_points.append(XYZ(x, y, z));
			}

		/**
		 * Deserialize a stroke point message.
		 * Check the actual number of bytes read with the length() method.
		 * @param data input device to read
		 * @param len length of the message
		 * @return new StrokePoint
		 */
		static StrokePoint *deserialize(QIODevice& data, int len);

		/**
		 * Add a new point to the message. A single message may contain
		 * at most (2^16 - 4) / 5 = 13106 points. (Although in actuality,
		 * most programs will run out of buffer space much earlier)
		 * @param x stroke X coordinate
		 * @param y stroke Y coordinate
		 * @param z stroke pressure
		 */
		void addPoint(int x, int y, int z) {
			_points.append(XYZ(x, y, z));
		}

		/**
		 * Get the number of points contained in this message.
		 * Most messages contain only one point.
		 * @return number of points
		 */
		int points() const { return _points.count(); }

		/**
		 * Get the length of the message payload
		 * @return payload length in bytes
		 */
		unsigned int payloadLength() const { return 1 + _points.size() * 5; }

		/**
		 * Get the user from which this message was received
		 * @return user ID
		 */
		int user() const { return _user; }

		/**
		 * Get the coordinates for a stroke point
		 */
		XYZ point(int i) const { return _points[i]; }

	protected:
		void serializeBody(QIODevice& data) const;

	private:
		int _user;
		QVarLengthArray<XYZ, 1> _points;
};

/**
 * A stroke end message. This message is sent when the pen is lifted.
 */
class StrokeEnd : public Packet {
	public:
		StrokeEnd(int user) : Packet(STROKE_END), _user(user) { }

		/**
		 * Deserialize a stroke end message.
		 */
		static StrokeEnd *deserialize(QIODevice& data, int len);

		/**
		 * Get the message payload length
		 */
		unsigned int payloadLength() const { return 1; }

		/**
		 * Get the ID of the user who lifted their pen.
		 */
		int user() const { return _user; }

	protected:
		void serializeBody(QIODevice& data) const;

	private:
		int _user;
};

}

#endif

