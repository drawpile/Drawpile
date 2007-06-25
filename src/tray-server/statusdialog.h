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

#ifndef StatusDialog_INCLUDED
#define StatusDialog_INCLUDED

#include <QDialog>
#include <QString>

class QLabel;
class QGroupBox;

//! Server status dialog
class StatusDialog
	: public QDialog
{
	Q_OBJECT
	
protected:
	// these might need to be updated occasionally
	QLabel *state_text;
	QGroupBox *session_group;
	QGroupBox *user_group;
	
	// constant
	const QString unknown_value;
	
public:
	StatusDialog();
};

#endif // StatusDialog_INCLUDED
