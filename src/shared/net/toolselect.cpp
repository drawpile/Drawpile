#include <QIODevice>

#include "toolselect.h"
#include "utils.h"

namespace protocol {

ToolSelect *ToolSelect::deserialize(QIODevice& data, int len) {
	Q_ASSERT(len == 16);
	int user = Utils::read8(data);
	int tool = Utils::read8(data);
	int mode = Utils::read8(data);
	quint32 c1 = Utils::read32(data);
	quint32 c0 = Utils::read32(data);
	int s1 = Utils::read8(data);
	int s0 = Utils::read8(data);
	int h1 = Utils::read8(data);
	int h0 = Utils::read8(data);
	int space = Utils::read8(data);
	return new ToolSelect(user, tool, mode, c1, c0, s1, s0, h1, h0, space);
}

void ToolSelect::serializeBody(QIODevice& data) const {
	data.putChar(_user);
	data.putChar(_tool);
	data.putChar(_mode);
	Utils::write32(data, _c1);
	Utils::write32(data, _c0);
	data.putChar(_s1);
	data.putChar(_s0);
	data.putChar(_h1);
	data.putChar(_h0);
	data.putChar(_space);
}

}

