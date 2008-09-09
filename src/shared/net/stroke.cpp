#include <QIODevice>

#include "stroke.h"
#include "utils.h"

namespace protocol {

StrokePoint *StrokePoint::deserialize(QIODevice& data, int len) {
	char user = Utils::read8(data);
	quint16 x = Utils::read16(data);
	quint16 y = Utils::read16(data);
	quint8 z = Utils::read8(data);
	StrokePoint *sp = new StrokePoint(user, x, y ,z);

	// A strokepoint packet may contain multiple points
	for(int i=6;i<len;i+=5) {
		quint16 x = Utils::read16(data);
		quint16 y = Utils::read16(data);
		quint8 z = Utils::read8(data);
		sp->addPoint(x, y, z);
	}

	return sp;
}

void StrokePoint::serializeBody(QIODevice& data) const {
	data.putChar(_user);
	for(int i=0;i<_points.size();++i) {
		Utils::write16(data, _points[i].x);
		Utils::write16(data, _points[i].y);
		data.putChar(_points[i].z);
	}
}

StrokeEnd *StrokeEnd::deserialize(QIODevice& data, int len) {
	return new StrokeEnd(Utils::read8(data));
}

void StrokeEnd::serializeBody(QIODevice& data) const {
	data.putChar(_user);
}

}
