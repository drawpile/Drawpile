/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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
#ifndef REC_FILTER_H
#define REC_FILTER_H

class QString;

namespace recording {

class Reader;

struct FilterOptions {
	bool removeUndone;    // remove undone actions
	bool removeChat;      // remove chat messages
	bool removeLookyLoos; // remove users who didn't do anything
	bool removeDelays;    // Remove Interval messages
	bool removeLasers;    // Remove laser pointer usage
	bool removeMarkers;   // Remove Marker messages
	bool squishStrokes;   // Squish adjacent Draw*Dabs messages
};

bool filterRecording(const QString &inputFile, const QString &outputFile, const FilterOptions &options, QString *errorMessage=nullptr);

}

#endif
