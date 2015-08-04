/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#ifndef TABLETSTATE_H
#define TABLETSTATE_H

#include <QObject>

/**
 * @brief Current state of the drawing tablet
 *
 * This is used to work around the lack of tablet events in QtQuick
 * (and the general buggines of QTabletEvent)
 */
class TabletState : public QObject
{
	Q_PROPERTY(qreal lastPressure READ lastPressure NOTIFY lastPressureChanged)
	Q_PROPERTY(bool stylusDown READ isStylusDown NOTIFY stylusDownChanged)

	Q_OBJECT
public:
	explicit TabletState(QObject *parent = 0);

	qreal lastPressure() const { return m_lastpressure; }
	bool isStylusDown() const { return m_stylusdown; }

	// Note: these properties are read-only from QML side
	void setLastPressure(qreal p);
	void setStylusDown(bool d);

signals:
	void lastPressureChanged(qreal);
	void stylusDownChanged(bool);

private:
	qreal m_lastpressure;
	bool m_stylusdown;
};

#endif // TABLETSTATE_H
