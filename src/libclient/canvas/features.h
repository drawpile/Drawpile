/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#ifndef ACL_FEATURES_H
#define ACL_FEATURES_H

namespace canvas {

// Features with configurable access levels
// See also shared/net/meta2.{cpp,h}
enum class Feature {
	PutImage,
	RegionMove,
	Resize,
	Background,
	EditLayers,
	OwnLayers,
	CreateAnnotation,
	Laser,
	Undo
};

static const int FeatureCount = int(Feature::Undo)+1;

// Access levels
enum class Tier : unsigned char {
	Op,      // operators
	Trusted, // + users marked as trusted
	Auth,    // + registered users
	Guest    // everyone
};

static const int TierCount = 4;

}

#endif

