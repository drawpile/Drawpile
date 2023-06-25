// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NETSTATS_H
#define NETSTATS_H

#include <QDialog>

class QTimer;
class Ui_NetStats;

namespace dialogs {

class NetStats final : public QDialog {
	Q_OBJECT
public:
	explicit NetStats(QWidget *parent = nullptr);
	~NetStats() override;

	void updateMemoryUsage();

public slots:
	void setSentBytes(int bytes);
	void setRecvBytes(int bytes);
	void setCurrentLag(int lag);
	void setDisconnected();

private slots:
	void updateMemoryUsagePeriodic();

private:
	static QString formatDataSize(size_t bytes);

	Ui_NetStats *m_ui;
	QTimer *m_updateMemoryTimer;
};

}

#endif // NETSTATS_H
