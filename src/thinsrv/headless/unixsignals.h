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
#ifndef UNIXSIGNALS_H
#define UNIXSIGNALS_H

#include <QObject>

/**
 * @brief A class for converting UNIX signals to Qt signals
 *
 * To use, simply connect to the signal you are interested.
 */
class UnixSignals final : public QObject
{
	Q_OBJECT
public:
	static UnixSignals *instance();

signals:
	void sigInt();
	void sigTerm();
	void sigUsr1();

protected:
	void connectNotify(const QMetaMethod &signal) override;

private slots:
	void handleSignal();

private:
	UnixSignals();
};

#endif // UNIXSIGNALS_H
