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

#include "desktop/dialogs/netstats.h"
#include "ui_netstats.h"

namespace dialogs {

static QString formatKb(int bytes)
{
	if(bytes < 1024)
		return QStringLiteral("%1 b").arg(bytes);
	else if(bytes < 1024*1024)
		return QStringLiteral("%1 Kb").arg(bytes/1024);
	else
		return QStringLiteral("%1 Mb").arg(bytes/float(1024*1024), 0, 'f', 1);
}

NetStats::NetStats(QWidget *parent) :
	QDialog(parent), _ui(new Ui_NetStats)
{
	_ui->setupUi(this);
	setDisconnected();
}

NetStats::~NetStats()
{
	delete _ui;
}

void NetStats::setRecvBytes(int bytes)
{
	_ui->recvLabel->setText(formatKb(bytes));
}

void NetStats::setSentBytes(int bytes)
{
	_ui->sentLabel->setText(formatKb(bytes));
}

void NetStats::setCurrentLag(int lag)
{
	_ui->lagLabel->setText(QStringLiteral("%1 ms").arg(lag));
}

void NetStats::setDisconnected()
{
	_ui->lagLabel->setText(tr("not connected"));
}

}
