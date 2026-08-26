// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_ACTIVITYBROADCASTDIALOG_H
#define DESKTOP_DIALOGS_ACTIVITYBROADCASTDIALOG_H
#ifndef DP_HAVE_ACTIVITYBROADCAST
#	error "DP_HAVE_ACTIVITYBROADCAST undefined, should not include this header"
#endif
#include <QDialog>

class KisSliderSpinBox;
class QPushButton;
class QStackedWidget;
class QPlainTextEdit;

namespace net {
class ActivityBroadcast;
}

namespace dialogs {

class ActivityBroadcastDialog final : public QDialog {
	Q_OBJECT
	Q_DISABLE_COPY_MOVE(ActivityBroadcastDialog)
public:
	explicit ActivityBroadcastDialog(QWidget *parent = nullptr);

	net::ActivityBroadcast *activityBroadcast() { return m_activityBroadcast; }
	void setActivityBroadcast(net::ActivityBroadcast *activityBroadcast);

Q_SIGNALS:
	void activityBroadcastStarted();
	void activityBroadcastStopped();

private:
	void updatePage();

	void startActivityBroadcast();
	bool tryStart(QString &outError);
	void connectActivityBroadcast();
	void stopActivityBroadcast();

	void addBroadcastedActivityToLog(const QByteArray &bytes);

	net::ActivityBroadcast *m_activityBroadcast = nullptr;
	QStackedWidget *m_stack;
	QWidget *m_startPage;
	KisSliderSpinBox *m_portSpinner;
	QPushButton *m_startButton;
	QWidget *m_runPage;
	QPlainTextEdit *m_logEdit;
	QPushButton *m_stopButton;
};

}

#endif
