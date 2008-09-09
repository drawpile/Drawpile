#ifndef DP_PROTO_TOOLSELECT_H
#define DP_PROTO_TOOLSELECT_H

#include "protocol.h"

namespace protocol {

/**
 * A tool select message sets the user's tool settings.
 * ToolSelect messages are typically sent just before a StrokePoint,
 * but only if the tool has changed since the last one sent.
 *
 * The semantics of the settings are left to the client.
 */
class ToolSelect : public Packet {
	public:
		/**
		 * Construct a tool select.
		 * @param user user whose tool to change.
		 * @param tool tool ID.
		 * @param composition mode.
		 * @param c1 high-pressure color
		 * @param c0 low-pressure color
		 * @param s1 high-pressure size
		 * @param s0 low-pressure size
		 * @param h1 high-pressure hardness
		 * @param h0 low-pressure hardness
		 * @param space dab spacing
		 */
		ToolSelect(int user, int tool, int mode, quint32 c1, quint32 c0, int s1, int s0, int h1, int h0, int space) :
			Packet(TOOL_SELECT), _user(user), _tool(tool), _mode(mode),
			_c1(c1), _c0(c0), _s1(s1), _s0(s0), _h1(h1), _h0(h0), _space(space) { }

		/**
		 * Deserialize a tool select message
		 */
		static ToolSelect *deserialize(QIODevice& data, int len);

		/**
		 * Get the length of the tool select payload
		 */
		unsigned int payloadLength() const { return 16; }

		/**
		 * Get the ID of the user whose tool to change.
		 */
		int user() const { return _user; }

		/**
		 * Get the tool type
		 */
		int tool() const { return _tool; }

		/**
		 * Get the composition mode
		 */
		int mode() const { return _mode; }

		/**
		 * Get the high-pressure color
		 */
		quint32 c1() const { return _c1; }

		/**
		 * Get the low-pressure color
		 */
		quint32 c0() const { return _c0; }

		/**
		 * Get the high-pressure size
		 */
		int s1() const { return _s1; }

		/**
		 * Get the low-pressure size
		 */
		int s0() const { return _s0; }

		/**
		 * Get the high-pressure hardness
		 */
		int h1() const { return _h1; }

		/**
		 * Get the low-pressure hardness
		 */
		int h0() const { return _h0; }

		/**
		 * Get the dab spacing
		 */
		int spacing() const { return _space; }

	protected:
		void serializeBody(QIODevice& data) const;

	private:
		const int _user, _tool, _mode;
		const quint32 _c1, _c0;
		const int _s1, _s0, _h1, _h0, _space;
};

}

#endif

