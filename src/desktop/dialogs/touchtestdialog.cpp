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
		debugLogEvent(event, QString());
		event->accept();
		return true;
	case QEvent::Gesture:
		debugLogGestureEvent(static_cast<QGestureEvent *>(event));
		event->accept();
		return true;
	default:
		return QGraphicsView::viewportEvent(event);
	}
}

void TouchTestView::debugLogGestureEvent(QGestureEvent *event)
{
	QStringList gestureInfos;
	for(const QGesture *gesture : event->gestures()) {
		Qt::GestureType type = gesture->gestureType();
		QString typeName;
		QString extra;
		switch(type) {
		case Qt::TapGesture:
			typeName = QStringLiteral("Tap");
			extra = QStringLiteral(", data=%1")
						.arg(compat::debug(
							static_cast<const QTapGesture *>(gesture)));
			break;
		case Qt::TapAndHoldGesture:
			typeName = QStringLiteral("TapAndHold");
			extra = QStringLiteral(", data=%1")
						.arg(compat::debug(
							static_cast<const QTapAndHoldGesture *>(gesture)));
			break;
		case Qt::PanGesture:
			typeName = QStringLiteral("Pan");
			extra = QStringLiteral(", data=%1")
						.arg(compat::debug(
							static_cast<const QPanGesture *>(gesture)));
			break;
		case Qt::PinchGesture:
			typeName = QStringLiteral("Pinch");
			extra = QStringLiteral(", data=%1")
						.arg(compat::debug(
							static_cast<const QPinchGesture *>(gesture)));
			break;
		case Qt::SwipeGesture:
			typeName = QStringLiteral("Swipe");
			extra = QStringLiteral(", data=%1")
						.arg(compat::debug(
							static_cast<const QSwipeGesture *>(gesture)));
			break;
		case Qt::CustomGesture:
			typeName = QStringLiteral("Custom");
			break;
		default:
			typeName = QStringLiteral("0x%1").arg(QString::number(type, 16));
			break;
		}

		QGesture::GestureCancelPolicy cancelPolicy =
			gesture->gestureCancelPolicy();
		QString cancelPolicyName;
		switch(cancelPolicy) {
		case QGesture::CancelNone:
			cancelPolicyName = QStringLiteral("None");
			break;
		case QGesture::CancelAllInContext:
			cancelPolicyName = QStringLiteral("AllInContext");
			break;
		default:
			cancelPolicyName =
				QStringLiteral("0x%1").arg(QString::number(cancelPolicy, 16));
			break;
		}

		QString hotSpotName;
		if(gesture->hasHotSpot()) {
			QPointF hotSpot = gesture->hotSpot();
			hotSpotName =
				QStringLiteral("(%1,%2)").arg(hotSpot.x(), hotSpot.y());
		}

		Qt::GestureState state = gesture->state();
		QString stateName;
		switch(state) {
		case Qt::NoGesture:
			stateName = QStringLiteral("No");
			break;
		case Qt::GestureStarted:
			stateName = QStringLiteral("Started");
			break;
		case Qt::GestureUpdated:
			stateName = QStringLiteral("Updated");
			break;
		case Qt::GestureFinished:
			stateName = QStringLiteral("Finished");
			break;
		case Qt::GestureCanceled:
			stateName = QStringLiteral("Canceled");
			break;
		default:
			stateName = QStringLiteral("0x%1").arg(QString::number(state, 16));
			break;
		}

		gestureInfos.append(
			QStringLiteral(
				"QGesture(type=%1, cancelPolicy=%2, hotSpot=%3, state=%4%5)")
				.arg(
					typeName, cancelPolicyName, hotSpotName, stateName, extra));
	}
	debugLogEvent(
		event,
		QStringLiteral("%1 gesture(s): %2")
			.arg(QString::number(gestureInfos.size()), gestureInfos.join(' ')));
}

void TouchTestView::debugLogEvent(QEvent *event, const QString extraInfo)
{
	QString touchStatus = m_touchEventsEnabled ? QStringLiteral("touch")
											   : QStringLiteral("no touch");
	QString gestureStatus = m_gestureEventsEnabled
								? QStringLiteral("gestures")
								: QStringLiteral("no gestures");
	QString message =
		QStringLiteral("[%1, %2] %3\n")
			.arg(touchStatus, gestureStatus, compat::debug(event));
	if(!extraInfo.isEmpty()) {
		message += QStringLiteral(" ") + extraInfo;
	}
	emit logEvent(message);
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
