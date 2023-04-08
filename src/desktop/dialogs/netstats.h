// SPDX-License-Identifier: GPL-3.0-or-later

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
