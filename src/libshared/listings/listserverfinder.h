// SPDX-License-Identifier: GPL-3.0-or-later

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

