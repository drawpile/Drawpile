#ifndef DP_PROTO_UTILS_H
#define DP_PROTO_UTILS_H

#include <QtEndian>

namespace protocol {

/**
 * A few utilities to aid (de)serialization.
 */
struct Utils {
	/**
	 * Write 2 bytes in network byte order.
	 */
	static inline void write16(QIODevice& data, quint16 val) {
		val = qToBigEndian(val);
		data.write((char*)&val, 2);
	}

	/**
	 * Read 2 bytes in network byte order.
	 */
	static inline quint16 read16(QIODevice& data) {
		quint16 val;
		data.read((char*)&val, 2);
		return qFromBigEndian(val);
	}

	/**
	 * Write 4 bytes in network byte order.
	 */
	static inline void write32(QIODevice& data, quint32 val) {
		val = qToBigEndian(val);
		data.write((char*)&val, 4);
	}

	/**
	 * Read 4 bytes in network byte order
	 */
	static inline quint32 read32(QIODevice& data) {
		quint32 val;
		data.read((char*)&val, 4);
		return qFromBigEndian(val);
	}

	/**
	 * Get one byte
	 */
	static inline quint8 read8(QIODevice& data) {
		quint8 val;
		data.getChar((char*)&val);
		return val;
	}
};

}

#endif
