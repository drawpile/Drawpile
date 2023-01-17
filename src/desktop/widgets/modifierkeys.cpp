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

#include "modifierkeys.h"

#include <QHBoxLayout>
#include <QCheckBox>

namespace widgets {

#ifdef Q_OS_MACOS
#define MAC_OR_PC(mac, pc) (mac)
#else
#define MAC_OR_PC(mac, pc) (pc)
#endif

ModifierKeys::ModifierKeys(QWidget *parent)
	: QWidget(parent)
{
	auto *layout = new QHBoxLayout;
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);

	m_buttons[0] = new QCheckBox(MAC_OR_PC("⇧", "Shift"), this);
	m_buttons[1] = new QCheckBox(MAC_OR_PC("⌘", "Ctrl"), this);
	m_buttons[2] = new QCheckBox(MAC_OR_PC("⎇", "Alt"), this);
	m_buttons[3] = new QCheckBox(MAC_OR_PC("Ctrl", "Meta"), this);

	for(int i=0;i<4;++i)
		layout->addWidget(m_buttons[i]);
}

Qt::KeyboardModifiers ModifierKeys::modifiers() const
{
	return
		(m_buttons[0]->isChecked() ? Qt::ShiftModifier : Qt::NoModifier)
		| (m_buttons[1]->isChecked() ? Qt::ControlModifier : Qt::NoModifier)
		| (m_buttons[2]->isChecked() ? Qt::AltModifier : Qt::NoModifier)
		| (m_buttons[3]->isChecked() ? Qt::MetaModifier : Qt::NoModifier);
}

void ModifierKeys::setModifiers(Qt::KeyboardModifiers mods)
{
	m_buttons[0]->setChecked(mods.testFlag(Qt::ShiftModifier));
	m_buttons[1]->setChecked(mods.testFlag(Qt::ControlModifier));
	m_buttons[2]->setChecked(mods.testFlag(Qt::AltModifier));
	m_buttons[3]->setChecked(mods.testFlag(Qt::MetaModifier));
}

}

