// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/touchtestdialog.h"
#include "desktop/filewrangler.h"
#include "desktop/utils/qtguicompat.h"
#include "libshared/util/qtcompat.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QGestureEvent>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGridLayout>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QTouchEvent>
#include <QVBoxLayout>

namespace dialogs {

TouchTestView::TouchTestView(QWidget *parent)
	: QGraphicsView(parent)
	, m_scene(new QGraphicsScene(this))
{
	setBackgroundBrush(Qt::white);
	setScene(m_scene);
	QGraphicsSimpleTextItem *text = m_scene->addSimpleText(tr("Touch here."));
	text->setBrush(Qt::black);
	text->setScale(2.0);
}

void TouchTestView::setTouchEventsEnabled(bool enabled)
{
	if(enabled && !m_touchEventsEnabled) {
		emit logEvent(QStringLiteral("Enable touch events\n"));
		viewport()->setAttribute(Qt::WA_AcceptTouchEvents, true);
		m_touchEventsEnabled = true;
	} else if(!enabled && m_touchEventsEnabled) {
		emit logEvent(QStringLiteral("Disable touch events\n"));
		viewport()->setAttribute(Qt::WA_AcceptTouchEvents, false);
		m_touchEventsEnabled = false;
	}
}

void TouchTestView::setGestureEventsEnabled(bool enabled)
{
	if(enabled && !m_gestureEventsEnabled) {
		emit logEvent(QStringLiteral("Enable gesture events\n"));
		viewport()->grabGesture(Qt::PanGesture);
		viewport()->grabGesture(Qt::PinchGesture);
		viewport()->grabGesture(Qt::SwipeGesture);
		m_gestureEventsEnabled = true;
	} else if(!enabled && m_gestureEventsEnabled) {
		emit logEvent(QStringLiteral("Disable gesture events\n"));
		viewport()->ungrabGesture(Qt::PanGesture);
		viewport()->ungrabGesture(Qt::PinchGesture);
		viewport()->ungrabGesture(Qt::SwipeGesture);
		m_gestureEventsEnabled = false;
	}
}

bool TouchTestView::viewportEvent(QEvent *event)
{
	switch(event->type()) {
	case QEvent::TouchBegin:
	case QEvent::TouchUpdate:
	case QEvent::TouchEnd:
	case QEvent::TouchCancel:
	case QEvent::Gesture:
		emit logEvent(QStringLiteral("[%1, %2] %3\n")
						  .arg(
							  m_touchEventsEnabled ? QStringLiteral("touch")
												   : QStringLiteral("no touch"),
							  m_gestureEventsEnabled
								  ? QStringLiteral("gestures")
								  : QStringLiteral("no gestures"),
							  compat::debug(event)));
		return true;
	default:
		return QGraphicsView::viewportEvent(event);
	}
}


TouchTestDialog::TouchTestDialog(QWidget *parent)
	: QDialog(parent)
{
	setModal(true);
	resize(compat::widgetScreen(*this)->availableSize() * 0.8);

	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);

	QCheckBox *touchBox = new QCheckBox(tr("Enable touch"));
	QCheckBox *gestureBox = new QCheckBox(tr("Enable gestures"));
	touchBox->setChecked(true);
	gestureBox->setChecked(true);

	TouchTestView *ttv = new TouchTestView;
	QPlainTextEdit *edit = new QPlainTextEdit;
	edit->setReadOnly(true);
	connect(
		ttv, &TouchTestView::logEvent, edit, &QPlainTextEdit::appendPlainText);
	ttv->setTouchEventsEnabled(true);
	ttv->setGestureEventsEnabled(true);

	QGridLayout *grid = new QGridLayout;
	layout->addLayout(grid);
	grid->setColumnStretch(0, 3);
	grid->setColumnStretch(1, 1);
	grid->setColumnStretch(2, 1);
	grid->addWidget(ttv, 0, 0, 2, 1);
	grid->addWidget(touchBox, 0, 1, Qt::AlignHCenter);
	grid->addWidget(gestureBox, 0, 2, Qt::AlignHCenter);
	grid->addWidget(edit, 1, 1, 1, 2);

	connect(
		touchBox, &QAbstractButton::clicked, ttv,
		&TouchTestView::setTouchEventsEnabled);
	connect(
		gestureBox, &QAbstractButton::clicked, ttv,
		&TouchTestView::setGestureEventsEnabled);

	QDialogButtonBox *buttons = new QDialogButtonBox(
		QDialogButtonBox::Close | QDialogButtonBox::Reset |
			QDialogButtonBox::Save,
		this);
	layout->addWidget(buttons);
	connect(
		buttons, &QDialogButtonBox::clicked, this,
		[=](QAbstractButton *button) {
			if(button == buttons->button(QDialogButtonBox::Reset)) {
				edit->clear();
			} else if(button == buttons->button(QDialogButtonBox::Save)) {
				QString path = FileWrangler(this).getSaveLogFilePath();
				if(!path.isEmpty()) {
					QByteArray bytes = edit->toPlainText().toUtf8();
					QFile file(path);
					bool ok =
						file.open(QIODevice::WriteOnly | QIODevice::Text) &&
						file.write(bytes) && file.flush();
					if(!ok) {
						QMessageBox::critical(
							this, tr("Error Saving Log"),
							tr("Touch test log could not be saved: %1")
								.arg(file.errorString()));
					}
					file.close();
				}
			} else if(button == buttons->button(QDialogButtonBox::Close)) {
				reject();
			}
		});
}

}
