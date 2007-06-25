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

#ifndef ConfigDialog_INCLUDED
#define ConfigDialog_INCLUDED

#include <QDialog>

class QCheckBox;
class QSpinBox;

//! Server configuration/settings dialog
/**
 * @todo Actual behaviour for all the GUI widgets.
 * @todo Connect wide_strings checkbox to namelen_spinner
 */
class ConfigDialog
	: public QDialog
{
	Q_OBJECT
	
protected:
	// following are disabled when server is running
	QCheckBox *wide_strings;
	QCheckBox *unique_names;
	QSpinBox *port_spinner;
	
	// behaviour changes depending on other options
	QSpinBox *namelen_spinner;
	
	// command buttons
	QPushButton *apply_butt;
	QPushButton *save_butt;
	QPushButton *reset_butt;
	
public slots:
	//! Slot
	void serverStarted();
	void serverStopped();
	
public:
	void serverRunning(bool _true);
	
private slots:
	void wideStrChanged(int state);
	void enableButtons(bool _enable=true);
	
	void applyAction();
	void saveAction();
	void resetAction();
	
public:
	ConfigDialog();
};

#endif // ConfigDialog_INCLUDED
