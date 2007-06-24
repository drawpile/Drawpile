/******************************************************************************

   Copyright (C) 2007 M.K.A. <wyrmchild@users.sourceforge.net>

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

******************************************************************************/

#include "configdialog.h"

#include <QDebug>
#include <QtGui>

ConfigDialog::ConfigDialog()
{
	setWindowTitle(tr("DrawPile Server Configuration"));
	
	//setAttribute(Qt::WA_DeleteOnClose);
	
	// user limit
	
	QSpinBox *ulimit_spinner = new QSpinBox;
	ulimit_spinner->setRange(1, 255);
	ulimit_spinner->setValue(20);
	
	QHBoxLayout *ulimit_box = new QHBoxLayout;
	ulimit_box->addWidget(new QLabel(tr("User limit")), 1);
	ulimit_box->addWidget(ulimit_spinner, 0);
	
	// session limit
	
	QSpinBox *slimit_spinner = new QSpinBox;
	slimit_spinner->setRange(1, 255);
	slimit_spinner->setValue(5);
	
	QHBoxLayout *slimit_box = new QHBoxLayout;
	slimit_box->addWidget(new QLabel(tr("Session limit")), 1);
	slimit_box->addWidget(slimit_spinner, 0);
	
	// options
	
	
	
	// command box
	
	QHBoxLayout *command_box = new QHBoxLayout;
	QPushButton *apply_butt = new QPushButton(tr("Apply settings"));
	QPushButton *save_butt = new QPushButton(tr("Save settings"));
	command_box->addWidget(apply_butt);
	command_box->addWidget(save_butt);
	
	// root
	
	QVBoxLayout *root_layout = new QVBoxLayout;
	root_layout->setContentsMargins(3,3,3,3);
	root_layout->addStretch(1);
	root_layout->addSpacing(3);
	root_layout->addStrut(120);
	
	root_layout->addLayout(ulimit_box);
	root_layout->addLayout(slimit_box);
	
	root_layout->addLayout(command_box);
	
	setLayout(root_layout);
}
