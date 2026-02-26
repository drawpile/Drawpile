#include "libclient/utils/icons.h"

namespace utils {

Icons *Icons::instance;

void Icons::reset()
{
	Icons *icons = get();

	icons->m_alphaInherit = QIcon();
	icons->m_alphaInherit.addFile(
		QStringLiteral("theme:drawpile_alpha_on.svg"), QSize(), QIcon::Normal,
		QIcon::Off);
	icons->m_alphaInherit.addFile(
		QStringLiteral("theme:drawpile_alpha_off.svg"), QSize(), QIcon::Normal,
		QIcon::On);

	icons->m_alphaLock.addFile(
		QStringLiteral("theme:drawpile_alpha_unlocked.svg"), QSize(),
		QIcon::Normal, QIcon::Off);
	icons->m_alphaLock.addFile(
		QStringLiteral("theme:drawpile_alpha_locked.svg"), QSize(),
		QIcon::Normal, QIcon::On);
	icons->m_alphaLock.addFile(
		QStringLiteral("theme:drawpile_alpha_disabled.svg"), QSize(),
		QIcon::Disabled);
}

}
