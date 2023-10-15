// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/utils/mandatoryfields.h"

#include <QLineEdit>
#include <QComboBox>
#include <QVariant>
#include <QDebug>
#include <utility>

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
	for(QObject *obj : std::as_const(m_widgets)) {
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

