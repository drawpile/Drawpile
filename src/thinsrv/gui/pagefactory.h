/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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
#ifndef PAGEFACTORY_H
#define PAGEFACTORY_H

class QString;
class QWidget;

namespace server {
namespace gui {

class Server;

class PageFactory
{
public:
	PageFactory() = default;
	virtual ~PageFactory() = default;

	/**
	 * @brief Get the title of the page (shown in the sidebar)
	 */
	virtual QString title() const = 0;

	/**
	 * @brief Get the internal page identifier
	 */
	virtual QString pageId() const = 0;

	/**
	 * @brief Construct a new page
	 */
	virtual QWidget *makePage(Server *server) const = 0;
};

}
}

Q_DECLARE_METATYPE(server::gui::PageFactory*)

#endif
