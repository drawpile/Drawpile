/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2009 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#ifndef ORAREADER_H
#define ORAREADER_H

#include <QRect>

namespace dpcore {
	class LayerStack;
}

class Zipfile;
class QDomElement;
class QPoint;

namespace openraster {

class Reader {
	public:
		Reader(const QString& file);
		~Reader();

		dpcore::LayerStack *open() const;

	private:
		QRect computeBounds(const QDomElement& stack, QPoint offset) const;
		bool loadLayers(dpcore::LayerStack *layers, const QDomElement& stack, QPoint offset) const;

		Zipfile *ora;
};

}

#endif

