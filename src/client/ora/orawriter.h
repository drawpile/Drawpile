/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2009-2014 Calle Laakkonen

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
#ifndef ORAWRITER_H
#define ORAWRITER_H

#include <QList>

namespace paintcore {
	class LayerStack;
}

namespace openraster {

extern const QString DP_NAMESPACE;

/**
 * @brief Save the layer stack as an OpenRaster file
 *
 * @param filename target file path
 * @param image layer stack to save
 * @param errorMessage if not null, error message is put here
 * @return false on error
 */
bool saveOpenRaster(const QString &filename, const paintcore::LayerStack *image, QString *errorMessage=nullptr);

}

#endif

