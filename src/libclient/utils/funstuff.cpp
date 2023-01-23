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

#include "libclient/utils/funstuff.h"

#include <QRegularExpression>
#include <QVector>
#include <QRandomGenerator>

#include <cstdlib>

// TODO once extension scripting is implemented, this should be moved into a script.
namespace utils {

int diceRoll(int number, int faces)
{
	Q_ASSERT(number>0);
	Q_ASSERT(faces>0);

	int result = 0;
	while(number-->0) {
		result += QRandomGenerator::global()->generateDouble() * faces + 1;
	}

	return result;
}

DiceRoll diceRoll(const QString &rolltype)
{
	DiceRoll result;
	static QRegularExpression re("^([1-9]\\d*)?[dD]([1-9]\\d*)?([+-]\\d+)?$");
	auto m = re.match(rolltype);
	if(!m.hasMatch())
		return result;

	result.number = 1;
	result.faces = 6;
	if(!m.captured(1).isEmpty())
		result.number = m.captured(1).toInt();

	if(!m.captured(2).isEmpty())
		result.faces = m.captured(2).toInt();

	if(!m.captured(3).isEmpty())
		result.bias = m.captured(3).toInt();

	result.result = diceRoll(result.number, result.faces) + result.bias;
	return result;
}

#ifndef NDEBUG
QList<float> diceRollDistribution(const QString &rolltype)
{
	QList<float> distribution;
	QVector<int> rolls;
	const int ROLLS = 1000;

	for(int i=0;i<ROLLS;++i) {
		DiceRoll r = diceRoll(rolltype);
		if(r.number==0)
			return QList<float>();
		if(rolls.isEmpty())
			rolls.resize(r.number*r.faces);

		++rolls[r.result - 1 - r.bias];
	}

	for(int r : rolls)
		distribution.append(r / float(ROLLS));

	return distribution;
}
#endif

QString DiceRoll::toString() const
{
	QString s;
	if(bias) {
		if(bias>0)
			s = QStringLiteral("%2d%3+%1: %4");
		else
			s = QStringLiteral("%2d%3%1: %4");
		s = s.arg(bias);

	} else {
		s = QStringLiteral("%1d%2: %3");
	}

	return s.arg(number).arg(faces).arg(result);
}

}
