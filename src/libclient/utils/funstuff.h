// SPDX-License-Identifier: GPL-3.0-or-later

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
