/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include "utils.h"

#include <QString>

namespace docks {

	QString defaultDockStylesheet()
	{
#ifdef Q_OS_WIN
		return QStringLiteral(
			"QDockWidget {"
				"titlebar-close-icon: url(:/icons/builtin/dock-close.svg);"
				"titlebar-normal-icon: url(:/icons/builtin/dock-detach.svg);"

			"}"
		);
#else
		// Standard look is fine on Linux and OSX
		return QString();
#endif
	}
}
