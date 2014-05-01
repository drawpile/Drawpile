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
#include "mandatoryfields.h"

#include <QAbstractButton>
#include <QLineEdit>
#include <QComboBox>
#include <QVariant>
#include <QDebug>

/**
 * When constructed, all mandatory fields are searched recursively from
 * the parent object.
 * @param parent parent object of the mandatory fields
 * @param button button to disable when a field is blank
 */
MandatoryFields::MandatoryFields(QWidget *parent, QWidget *button)
	: QObject(parent), button_(button)
{
	collectFields(parent);
	// Collect a list of mandatory input fields
	changed();
}

//! Recursively collect mandatory fields
void MandatoryFields::collectFields(QObject *parent)
{
	foreach(QObject *obj, parent->children()) {
		if(obj->property("mandatoryfield").isValid()) {
			if(obj->inherits("QLineEdit")) {
				connect(obj, SIGNAL(textChanged(QString)), this, SLOT(changed()));
			} else if(obj->inherits("QComboBox")) {
				connect(obj, SIGNAL(editTextChanged(QString)), this, SLOT(changed()));
			} else {
				qWarning() << "unhandled mandatory field" << obj->metaObject()->className();
			}
			widgets_.append(obj);
		}
		collectFields(obj);
	}
}

void MandatoryFields::changed()
{
	bool enable = true;
	foreach(QObject *obj, widgets_) {
		if(obj->inherits("QLineEdit")) {
			if(static_cast<QLineEdit*>(obj)->text().isEmpty()) {
				enable = false;
				break;
			}
		} else if(obj->inherits("QComboBox")) {
			if(static_cast<QComboBox*>(obj)->currentText().isEmpty()) {
				enable = false;
				break;
			}
		}
	}
	button_->setEnabled(enable);
}

