#include <QIODevice>

#include "constants.h"
#include "login.h"
#include "utils.h"

namespace protocol {

LoginId::LoginId(const char magic[4], int rev, int ver) : Packet(LOGIN_ID),      _rev(rev), _ver(ver)
{
	_magic[0] = magic[0];
	_magic[1] = magic[1];
	_magic[2] = magic[2];
	_magic[3] = magic[3];
}

LoginId::LoginId(int ver) : Packet(LOGIN_ID), _rev(REVISION), _ver(ver) {
	_magic[0] = MAGIC[0];
	_magic[1] = MAGIC[1];
	_magic[2] = MAGIC[2];
	_magic[3] = MAGIC[3];
}

LoginId *LoginId::deserialize(QIODevice& data, int len) {
	char magic[4];
	data.read(magic, 4);
	int rev = Utils::read16(data);
	int ver = Utils::read16(data);
	return new LoginId(magic, rev, ver);
}

void LoginId::serializeBody(QIODevice& data) const {
	data.write(_magic, 4);
	Utils::write16(data, _rev);
	Utils::write16(data, _ver);
}

bool LoginId::isCompatible() const {
	return _magic[0] == MAGIC[0] &&
			_magic[1] == MAGIC[1] &&
			_magic[2] == MAGIC[2] &&
			_magic[3] == MAGIC[3] &&
			REVISION == _rev;
}

}

