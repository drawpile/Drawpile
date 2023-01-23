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

#include "desktop/widgets/keysequenceedit.h"

#include <QToolButton>
#include <QHBoxLayout>
#include <QKeySequenceEdit>

namespace widgets {

KeySequenceEdit::KeySequenceEdit(QWidget *parent) : QWidget(parent)
{
	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	QToolButton *clearButton = new QToolButton(this);
	clearButton->setText("⌫");

	_edit = new QKeySequenceEdit(this);
	connect(clearButton, &QToolButton::clicked, _edit, &QKeySequenceEdit::clear);
	connect(_edit, &QKeySequenceEdit::editingFinished, this, &KeySequenceEdit::editingFinished);

	layout->addWidget(_edit);
	layout->addWidget(clearButton);

	setFocusProxy(_edit);
}

void KeySequenceEdit::setKeySequence(const QKeySequence &ks)
{
	_edit->setKeySequence(ks);
}

QKeySequence KeySequenceEdit::keySequence() const
{
	return _edit->keySequence();
}

}
