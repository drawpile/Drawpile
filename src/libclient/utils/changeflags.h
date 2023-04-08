// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CHANGEFLAGS_H
#define CHANGEFLAGS_H

/**
 * A set of changes to a bitfield
 */
template<typename Flags>
class ChangeFlags {
public:
	ChangeFlags() : m_mask(0), m_changes(0) { }

	ChangeFlags &set(Flags flag, bool value)
	{
		m_mask |= flag;
		if(value)
			m_changes |= flag;
		else
			m_changes &= ~flag;

		return *this;
	}

	/**
	 * Return the given bitfield with the changes applied
	 */
	Flags update(Flags oldFlags) const
	{
		return (oldFlags & ~m_mask) | m_changes;
	}

private:
	Flags m_mask;
	Flags m_changes;
};

#endif

