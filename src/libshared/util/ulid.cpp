/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

// An implementation of Universally Unique Lexicographically Sortable Identifier
// https://github.com/ulid/spec

#include "ulid.h"

#include <QDateTime>
#include <QtEndian>
#include <cstring>
#include <QRandomGenerator>

namespace {
	// Crockford's base32 encoding
	static const char ENC[] = "0123456789abcdefghjkmnpqrstvwxyz";

	static const uchar DEC[] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

		// 0     1     2     3     4     5     6     7 
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
		// 8     9
		0x08, 0x09, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

		//    10(A) 11(B) 12(C) 13(D) 14(E) 15(F) 16(G)
		0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
		//17(H)     18(J) 19(K)       20(M) 21(N)
		0x11, 0xFF, 0x12, 0x13, 0xFF, 0x14, 0x15, 0xFF,
		//22(P)23(Q)24(R) 25(S) 26(T)       27(V) 28(W)
		0x16, 0x17, 0x18, 0x19, 0x1A, 0xFF, 0x1B, 0x1C,
		//29(X)30(Y)31(Z)
		0x1D, 0x1E, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,

		//    10(a) 11(b) 12(c) 13(d) 14(e) 15(f) 16(g)
		0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
		//17(h)     18(j) 19(k)       20(m) 21(n)
		0x11, 0xff, 0x12, 0x13, 0xff, 0x14, 0x15, 0xff,
		//22(p)23(q)24(r) 25(s) 26(t)       27(v) 28(w)
		0x16, 0x17, 0x18, 0x19, 0x1a, 0xff, 0x1b, 0x1c,
		//29(x)30(y)31(z)
		0x1d, 0x1e, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff,
	};
}

Ulid::Ulid()
{
	memset(m_ulid, 0, sizeof(m_ulid));
}

Ulid::Ulid(const QString &string)
	: Ulid()
{
	const auto str = string.toLatin1();

	// Validate
	if(str.length() != 26)
		return;
	for(int i=0;i<str.length();++i) {
		const uchar c = str.at(i);
		if(c > sizeof(DEC) || DEC[c] == 0xff)
			return;
	}

	// Timestamp
	m_ulid[0] = (DEC[int(str[0])] << 5) | DEC[int(str[1])];
	m_ulid[1] = (DEC[int(str[2])] << 3) | (DEC[int(str[3])] >> 2);
	m_ulid[2] = (DEC[int(str[3])] << 6) | (DEC[int(str[4])] << 1) | (DEC[int(str[5])] >> 4);
	m_ulid[3] = (DEC[int(str[5])] << 4) | (DEC[int(str[6])] >> 1);
	m_ulid[4] = (DEC[int(str[6])] << 7) | (DEC[int(str[7])] << 2) | (DEC[int(str[8])] >> 3);
	m_ulid[5] = (DEC[int(str[8])] << 5) | DEC[int(str[9])];

	// Random bits
	m_ulid[6] = (DEC[int(str[10])] << 3) | (DEC[int(str[11])] >> 2);
	m_ulid[7] = (DEC[int(str[11])] << 6) | (DEC[int(str[12])] << 1) | (DEC[int(str[13])] >> 4);
	m_ulid[8] = (DEC[int(str[13])] << 4) | (DEC[int(str[14])] >> 1);
	m_ulid[9] = (DEC[int(str[14])] << 7) | (DEC[int(str[15])] << 2) | (DEC[int(str[16])] >> 3);
	m_ulid[10] = (DEC[int(str[16])] << 5) | DEC[int(str[17])];
	m_ulid[11] = (DEC[int(str[18])] << 3) | (DEC[int(str[19])] >> 2);
	m_ulid[12] = (DEC[int(str[19])] << 6) | (DEC[int(str[20])] << 1) | (DEC[int(str[21])] >> 4);
	m_ulid[13] = (DEC[int(str[21])] << 4) | (DEC[int(str[22])] >> 1);
	m_ulid[14] = (DEC[int(str[22])] << 7) | (DEC[int(str[23])] << 2) | (DEC[int(str[24])] >> 3);
	m_ulid[15] = (DEC[int(str[24])] << 5) | DEC[int(str[25])];
}

bool Ulid::isNull() const
{
	for(size_t i=0;i<sizeof(m_ulid);++i) {
		if(m_ulid[i] != 0)
			return false;
	}
	return true;
}

Ulid Ulid::make()
{
	return make(QDateTime::currentDateTime());
}

Ulid Ulid::make(const QDateTime &datetime)
{
	Ulid u;

	// the first 6 bytes (48 bits) is the timestamp
	const qint64 timestamp = datetime.toMSecsSinceEpoch() << 16;
	qToBigEndian(timestamp, u.m_ulid);

	// The remaining 10 bytes (80 bits) are random
	quint32 randomWords[3];
	QRandomGenerator::global()->fillRange(randomWords);

	memcpy(u.m_ulid + 6, &randomWords, 10);

	return u;
}

QDateTime Ulid::timestamp() const
{
	return QDateTime::fromMSecsSinceEpoch(qFromBigEndian<quint64>(m_ulid) >> 16);
}

QString Ulid::toString() const
{
	// Implementation taken from https://github.com/suyash/ulid/blob/master/ulid_struct.hh
	// originally from https://sourcegraph.com/github.com/oklog/ulid@0774f81f6e44af5ce5e91c8d7d76cf710e889ebb/-/blob/ulid.go#L162-190
	const uchar *u = m_ulid;
	char str[27] = {
		// Timestamp (first 10 characters)
		ENC[(u[0] & 224) >> 5],
		ENC[u[0] & 31],
		ENC[(u[1] & 248) >> 3],
		ENC[((u[1] & 7) << 2) | ((u[2] & 192) >> 6)],
		ENC[(u[2] & 62) >> 1],
		ENC[((u[2] & 1) << 4) | ((u[3] & 240) >> 4)],
		ENC[((u[3] & 15) << 1) | ((u[4] & 128) >> 7)],
		ENC[(u[4] & 124) >> 2],
		ENC[((u[4] & 3) << 3) | ((u[5] & 224) >> 5)],
		ENC[u[5] & 31],

		// Random bits (last 16 characters)
		ENC[(u[6] & 248) >> 3],
		ENC[((u[6] & 7) << 2) | ((u[7] & 192) >> 6)],
		ENC[(u[7] & 62) >> 1],
		ENC[((u[7] & 1) << 4) | ((u[8] & 240) >> 4)],
		ENC[((u[8] & 15) << 1) | ((u[9] & 128) >> 7)],
		ENC[(u[9] & 124) >> 2],
		ENC[((u[9] & 3) << 3) | ((u[10] & 224) >> 5)],
		ENC[u[10] & 31],
		ENC[(u[11] & 248) >> 3],
		ENC[((u[11] & 7) << 2) | ((u[12] & 192) >> 6)],
		ENC[(u[12] & 62) >> 1],
		ENC[((u[12] & 1) << 4) | ((u[13] & 240) >> 4)],
		ENC[((u[13] & 15) << 1) | ((u[14] & 128) >> 7)],
		ENC[(u[14] & 124) >> 2],
		ENC[((u[14] & 3) << 3) | ((u[15] & 224) >> 5)],
		ENC[u[15] & 31],
		0
	};
	return QString::fromLatin1(str);
}

bool Ulid::operator==(const Ulid &other) const
{
	return memcmp(m_ulid, other.m_ulid, sizeof(m_ulid)) == 0;
}

bool Ulid::operator<(const Ulid &other) const
{
	return memcmp(m_ulid, other.m_ulid, sizeof(m_ulid)) < 0;
}

bool Ulid::operator>(const Ulid &other) const
{
	return memcmp(m_ulid, other.m_ulid, sizeof(m_ulid)) > 0;
}
