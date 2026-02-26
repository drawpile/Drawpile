// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_ICONS
#define LIBCLIENT_UTILS_ICONS
#include <QIcon>

namespace utils {

class Icons {
public:
	static void init()
	{
		if(!instance) {
			instance = new Icons;
		}
	}

	static void reset();

	static const QIcon &alphaInherit() { return get()->m_alphaInherit; }
	static const QIcon &alphaLock() { return get()->m_alphaLock; }

private:
	Icons() = default;

	static Icons *instance;

	static Icons *get()
	{
		Q_ASSERT(instance);
		return instance;
	}

	QIcon m_alphaInherit;
	QIcon m_alphaLock;
};

}

#endif
