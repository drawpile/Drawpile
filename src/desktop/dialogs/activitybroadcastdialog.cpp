// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/activitybroadcastdialog.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "libclient/net/activitybroadcast.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace dialogs {

ActivityBroadcastDialog::ActivityBroadcastDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(QStringLiteral("UDP Activity Stream"));
	setWindowModality(Qt::NonModal);
	resize(400, 300);

	QVBoxLayout *dlgLayout = new QVBoxLayout(this);

	m_stack = new QStackedWidget;
	dlgLayout->addWidget(m_stack, 1);

	m_startPage = new QWidget;
	m_startPage->setContentsMargins(0, 0, 0, 0);
	m_stack->addWidget(m_startPage);

	QFormLayout *startForm = new QFormLayout(m_startPage);
	startForm->setContentsMargins(0, 0, 0, 0);

	QLabel *m_explanationLabel = new QLabel;
	m_explanationLabel->setTextFormat(Qt::PlainText);
	m_explanationLabel->setWordWrap(true);
	m_explanationLabel->setText(QStringLiteral(
		"You can stream your activity in Drawpile via a UDP socket to attach "
		"it to streaming avatars, time trackers, sound makers or similar."));
	startForm->addRow(m_explanationLabel);

	m_portSpinner = new KisSliderSpinBox;
	m_portSpinner->setIndeterminate(true);
	m_portSpinner->setRange(0, 65535);
	m_portSpinner->setValue(27752);
	startForm->addRow(QStringLiteral("UDP Port:"), m_portSpinner);

	m_startButton = new QPushButton(QStringLiteral("Start"));
	startForm->addRow(m_startButton);
	connect(
		m_startButton, &QPushButton::clicked, this,
		&ActivityBroadcastDialog::startActivityBroadcast);

	m_runPage = new QWidget;
	m_runPage->setContentsMargins(0, 0, 0, 0);
	m_stack->addWidget(m_runPage);

	QVBoxLayout *runLayout = new QVBoxLayout(m_runPage);
	runLayout->setContentsMargins(0, 0, 0, 0);

	m_logEdit = new QPlainTextEdit;
	m_logEdit->setReadOnly(true);
	runLayout->addWidget(m_logEdit, 1);

	m_stopButton = new QPushButton(QStringLiteral("Stop"));
	runLayout->addWidget(m_stopButton);
	connect(
		m_stopButton, &QPushButton::clicked, this,
		&ActivityBroadcastDialog::stopActivityBroadcast);

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
	dlgLayout->addWidget(buttons);
	connect(
		buttons, &QDialogButtonBox::accepted, this,
		&ActivityBroadcastDialog::accept);
	connect(
		buttons, &QDialogButtonBox::rejected, this,
		&ActivityBroadcastDialog::reject);

	updatePage();
}

void ActivityBroadcastDialog::setActivityBroadcast(
	net::ActivityBroadcast *activityBroadcast)
{
	m_activityBroadcast = activityBroadcast;
	if(m_activityBroadcast) {
		connectActivityBroadcast();
	}
	updatePage();
}

void ActivityBroadcastDialog::updatePage()
{
	m_stack->setCurrentWidget(m_activityBroadcast ? m_runPage : m_startPage);
}

void ActivityBroadcastDialog::startActivityBroadcast()
{
	QString error;
	if(tryStart(error)) {
		m_logEdit->clear();
		connectActivityBroadcast();
		Q_EMIT activityBroadcastStarted();
	} else {
		utils::showCritical(
			this, QStringLiteral("Error"),
			QStringLiteral("Failed to start UDP activity stream."), error);
	}
	updatePage();
}

bool ActivityBroadcastDialog::tryStart(QString &outError)
{
	if(m_activityBroadcast) {
		outError = QStringLiteral("UDP activity stream is already running.");
		return false;
	}

	m_activityBroadcast = new net::ActivityBroadcast;
	if(!m_activityBroadcast->start(m_portSpinner->value(), &outError)) {
		m_activityBroadcast = nullptr;
		delete m_activityBroadcast;
		return false;
	}

	return true;
}

void ActivityBroadcastDialog::connectActivityBroadcast()
{
	connect(
		m_activityBroadcast, &net::ActivityBroadcast::activityBroadcasted, this,
		&ActivityBroadcastDialog::addBroadcastedActivityToLog);
}

void ActivityBroadcastDialog::stopActivityBroadcast()
{
	if(m_activityBroadcast) {
		m_activityBroadcast->stop();
		delete m_activityBroadcast;
		m_activityBroadcast = nullptr;
		Q_EMIT activityBroadcastStopped();
	}
	updatePage();
}

void ActivityBroadcastDialog::addBroadcastedActivityToLog(
	const QByteArray &bytes)
{
	if(!bytes.isEmpty()) {
		QString text = QString::fromUtf8(bytes);
		QTextDocument *document = m_logEdit->document();
		if(document && !document->isEmpty()) {
			m_logEdit->appendPlainText(QStringLiteral("\n") + text);
		} else {
			m_logEdit->setPlainText(text);
		}
	}
}

}
