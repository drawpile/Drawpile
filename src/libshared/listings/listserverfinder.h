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

#ifndef LIST_SERVER_FINDER_H
#define LIST_SERVER_FINDER_H

#include <QString>

class QIODevice;

namespace sessionlisting {

/**
 * Given a HTML file, find the link to a drawpile listserver in the meta headers.
 * The content of the first <meta> header with the name "drawpile:list-server" is returned.
 * If the file is not valid HTML or does not contain the header, a blank string is returned.
 */
QString findListserverLinkHtml(QIODevice *htmlFile);

}

#endif

