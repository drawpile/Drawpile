/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#ifndef NETSTATS_H
#define NETSTATS_H

#include <QDialog>

class Ui_NetStats;

namespace dialogs {

class NetStats final : public QDialog
{
	Q_OBJECT
public:
	explicit NetStats(QWidget *parent = nullptr);
	~NetStats() override;

public slots:
	void setSentBytes(int bytes);
	void setRecvBytes(int bytes);
	void setCurrentLag(int lag);
	void setDisconnected();

private:
	Ui_NetStats *_ui;
};

}

#endif // NETSTATS_H
