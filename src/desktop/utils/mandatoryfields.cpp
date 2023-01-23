/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2007-2019 Calle Laakkonen

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
#include "desktop/utils/mandatoryfields.h"

#include <QLineEdit>
#include <QComboBox>
#include <QVariant>
#include <QDebug>

/**
 * When constructed, all mandatory fields are searched recursively from
 * the parent object.
 * @param parent parent object of the mandatory fields
 * @param okButton button to disable when a field is blank
 */
MandatoryFields::MandatoryFields(QWidget *parent, QWidget *okButton)
	: QObject(parent), m_okButton(okButton)
{
	Q_ASSERT(parent);
	Q_ASSERT(okButton);
	collectFields(parent);
	update();
}

//! Recursively collect mandatory fields
void MandatoryFields::collectFields(QObject *parent)
{
	for(QObject *obj : parent->children()) {
		if(obj->property("mandatoryfield").isValid()) {
			if(obj->inherits("QLineEdit")) {
				connect(obj, SIGNAL(textChanged(QString)), this, SLOT(update()));
			} else if(obj->inherits("QComboBox")) {
				connect(obj, SIGNAL(editTextChanged(QString)), this, SLOT(update()));
			} else {
				qWarning() << "unhandled mandatory field" << obj->metaObject()->className();
			}
			m_widgets.append(obj);
		}
		collectFields(obj);
	}
}

void MandatoryFields::update()
{
	bool enable = true;
	for(QObject *obj : qAsConst(m_widgets)) {
		if(!obj->property("mandatoryfield").toBool())
			continue;

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
	m_okButton->setEnabled(enable);
}

