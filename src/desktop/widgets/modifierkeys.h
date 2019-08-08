/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#ifndef MODIFIERKEYS_H
#define MODIFIERKEYS_H

#include <QWidget>

#ifdef DESIGNER_PLUGIN
#include <QtUiPlugin/QDesignerExportWidget>
#else
#define QDESIGNER_WIDGET_EXPORT
#endif

class QAbstractButton;

namespace widgets {

class QDESIGNER_WIDGET_EXPORT ModifierKeys : public QWidget
{
	Q_OBJECT
	Q_PROPERTY(Qt::KeyboardModifiers modifiers READ modifiers WRITE setModifiers)
public:
	explicit ModifierKeys(QWidget *parent = nullptr);

	Qt::KeyboardModifiers modifiers() const;

signals:

public slots:
	void setModifiers(Qt::KeyboardModifiers modifiers);

private:
	QAbstractButton *m_buttons[4];
};

}

#endif // MODIFIERKEYS_H
