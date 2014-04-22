/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#ifndef UNIXSIGNALS_H
#define UNIXSIGNALS_H

#include <QObject>

/**
 * @brief A class for converting UNIX signals to Qt signals
 *
 * To use, simply connect to the signal you are interested.
 */
class UnixSignals : public QObject
{
	Q_OBJECT
public:
	static UnixSignals *instance();

signals:
	void sigInt();

protected:
	void connectNotify(const QMetaMethod &signal);

private slots:
	void handleSignal();

private:
	UnixSignals();
};

#endif // UNIXSIGNALS_H
