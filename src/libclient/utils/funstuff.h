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

#ifndef FUNSTUFF_H
#define FUNSTUFF_H

#include <QList>

class QString;

namespace utils {

struct DiceRoll {
	int number;
	int faces;
	int bias;
	int result;

	DiceRoll() : number(0), faces(0), bias(0), result(0) {}
	QString toString() const;
};

/**
 * @brief Roll n dices with f faces each.
 * @param number number of dice to roll
 * @param faces number of faces on each die
 * @return sum of the dice roll
 */
int diceRoll(int number, int faces);

/**
 * @brief Parse dice notation string and roll the dice
 *
 * Supported notation is [A]dX[±B], where A is the number of dice,
 * X is the number of faces and B is a constant added to the result.
 *
 * @param rolltype
 * @return dice roll result (with all values set to 0 on error)
 */
DiceRoll diceRoll(const QString &rolltype);

#ifndef NDEBUG
QList<float> diceRollDistribution(const QString &rolltype);
#endif

}

#endif // FUNSTUFF_H
