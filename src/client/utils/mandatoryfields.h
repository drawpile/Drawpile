/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

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
#ifndef MANDATORYFIELDS_H
#define MANDATORYFIELDS_H

#include <QObject>

class QWidget;

/**
 * A utility class that monitors the widgets on a dialog and
 * automatically disables the ok button when any widget
 * with a "mandatoryfield" property is unset.
 */
class MandatoryFields : public QObject {
	Q_OBJECT
	public:
		//! Construct the mandatory field monitor
		MandatoryFields(QWidget *parent, QWidget *button);

	private slots:
		void changed();

	private:
		void collectFields(QObject *parent);

		QList<QObject*> widgets_;
		QWidget *button_;
};

#endif

