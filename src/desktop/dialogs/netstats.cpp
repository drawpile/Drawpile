// SPDX-License-Identifier: GPL-3.0-or-later

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
