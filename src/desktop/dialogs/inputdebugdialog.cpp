// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/inputdebugdialog.h"
#include "desktop/utils/widgetutils.h"
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QtMath>

namespace dialogs {

#if DRAWPILE_INPUT_DEBUG_DIALOG_ANDROID
namespace {
static int inputDebugDialogCount;
}
#endif

InputDebugDialog::InputDebugDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(QStringLiteral("Input Debug"));
	utils::makeModal(this);
	resize(800, 600);

	QVBoxLayout *dlgLayout = new QVBoxLayout(this);

#if DRAWPILE_INPUT_DEBUG_DIALOG_ANDROID
	m_stateTable = new QTableWidget(0, 2);
	m_stateTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_stateTable->verticalHeader()->hide();
	m_stateTable->setHorizontalHeaderLabels(
		{QStringLiteral("Key"), QStringLiteral("Value")});
	m_stateTable->horizontalHeader()->setSectionResizeMode(
		QHeaderView::Stretch);
	dlgLayout->addWidget(m_stateTable);
	utils::bindKineticScrolling(m_stateTable);

	QCoreApplication::setKritaAttribute(
		KRITA_QATTRIBUTE_ANDROID_DEBUG_INPUT_EVENTS, true);
	connect(
		QCoreApplication::instance(), &QCoreApplication::debugAndroidInputEvent,
		this, &InputDebugDialog::handleDebugEvent, Qt::QueuedConnection);
	inputDebugDialogCount = qMax(1, inputDebugDialogCount + 1);
#else
	m_unsupportedLabel = new QLabel;
	m_unsupportedLabel->setAlignment(Qt::AlignCenter);
	m_unsupportedLabel->setWordWrap(true);
#	ifdef Q_OS_ANDROID
	m_unsupportedLabel->setText(QStringLiteral(
		"Input debugging is not available with this build of Qt."));
#	else
	m_unsupportedLabel->setText(
		QStringLiteral("Input debugging is not available on this platform."));
#	endif
	dlgLayout->addWidget(m_unsupportedLabel, 1);
#endif

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
	dlgLayout->addWidget(buttons);
	connect(
		buttons, &QDialogButtonBox::accepted, this, &InputDebugDialog::accept);
	connect(
		buttons, &QDialogButtonBox::rejected, this, &InputDebugDialog::reject);
}

#if DRAWPILE_INPUT_DEBUG_DIALOG_ANDROID
InputDebugDialog::~InputDebugDialog()
{
	inputDebugDialogCount = qMax(0, inputDebugDialogCount - 1);
	if(inputDebugDialogCount == 0) {
		QCoreApplication::setKritaAttribute(
			KRITA_QATTRIBUTE_ANDROID_DEBUG_INPUT_EVENTS, false);
	}
}

void InputDebugDialog::handleDebugEvent(const QVariantHash &eventData)
{
	int rowCount = 0;

	addStateRowString(eventData, QStringLiteral("deviceId"), rowCount);
	addStateRowString(eventData, QStringLiteral("action"), rowCount);
	addStateRowString(eventData, QStringLiteral("actionMasked"), rowCount);
	addStateRowString(eventData, QStringLiteral("eventTime"), rowCount);
	addStateRowString(eventData, QStringLiteral("buttonState"), rowCount);
	addStateRowString(eventData, QStringLiteral("metaState"), rowCount);
	addStateRowString(eventData, QStringLiteral("pointerCount"), rowCount);

	QVariantList pointers =
		eventData.value(QStringLiteral("pointers")).toList();
	int pointerCount = pointers.size();
	for(int i = 0; i < pointerCount; ++i) {
		QVariantHash pointer = pointers[i].toHash();
		addStateRowPointer(pointer, i, QStringLiteral("pointerId"), rowCount);

		QVariantHash axes = pointer.value(QStringLiteral("axes")).toHash();
		addStateRowAxis(axes, i, QStringLiteral("BRAKE"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("DISTANCE"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GAS"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GENERIC_1"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GENERIC_2"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GENERIC_3"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GENERIC_4"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GENERIC_5"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GENERIC_6"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GENERIC_7"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GENERIC_8"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GENERIC_9"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GENERIC_10"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GENERIC_11"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GENERIC_12"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GENERIC_13"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GENERIC_14"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GENERIC_15"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("GENERIC_16"), true, rowCount);
		addStateRowAxis(
			axes, i, QStringLiteral("GESTURE_PINCH_SCALE_FACTOR"), false,
			rowCount);
		addStateRowAxis(
			axes, i, QStringLiteral("GESTURE_SCROLL_X_DISTANCE"), false,
			rowCount);
		addStateRowAxis(
			axes, i, QStringLiteral("GESTURE_SCROLL_Y_DISTANCE"), false,
			rowCount);
		addStateRowAxis(
			axes, i, QStringLiteral("GESTURE_X_OFFSET"), false, rowCount);
		addStateRowAxis(
			axes, i, QStringLiteral("GESTURE_Y_OFFSET"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("HAT_X"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("HAT_Y"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("HSCROLL"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("LTRIGGER"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("ORIENTATION"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("PRESSURE"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("RELATIVE_X"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("RELATIVE_Y"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("RTRIGGER"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("RUDDER"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("RX"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("RY"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("RZ"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("SCROLL"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("SIZE"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("THROTTLE"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("TILT"), true, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("TOOL_MAJOR"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("TOOL_MINOR"), false, rowCount);
		addStateRowAxis(
			axes, i, QStringLiteral("TOUCH_MAJOR"), false, rowCount);
		addStateRowAxis(
			axes, i, QStringLiteral("TOUCH_MINOR"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("VSCROLL"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("WHEEL"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("X"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("Y"), false, rowCount);
		addStateRowAxis(axes, i, QStringLiteral("Z"), false, rowCount);
	}

	while(m_stateTable->rowCount() > rowCount) {
		m_stateTable->removeRow(rowCount);
	}
}

void InputDebugDialog::addStateRowString(
	const QVariantHash &h, const QString &key, int &inOutRowCount)
{
	addStateRow(key, h.value(key).toString(), inOutRowCount);
}

void InputDebugDialog::addStateRowPointer(
	const QVariantHash &h, int pointerIndex, const QString &pointerKey,
	int &inOutRowCount)
{
	QString key =
		QStringLiteral("%1.%2").arg(QString::number(pointerIndex), pointerKey);
	addStateRow(key, h.value(pointerKey).toString(), inOutRowCount);
}

void InputDebugDialog::addStateRowAxis(
	const QVariantHash &h, int pointerIndex, const QString &axis,
	bool withDegrees, int &inOutRowCount)
{
	QString key =
		QStringLiteral("%1.%2").arg(QString::number(pointerIndex), axis);
	QString value;
	if(QVariant v = h.value(axis); v.isValid()) {
		float f = v.toFloat();
		value = QString::number(f, 'f');
		if(withDegrees) {
			value.append(
				QStringLiteral(" (%2°)").arg(qRadiansToDegrees(f), 0, 'f'));
		}
	}
	addStateRow(key, value, inOutRowCount);
}

void InputDebugDialog::addStateRow(
	const QString &key, const QString &value, int &inOutRowCount)
{
	int tableRowCount = m_stateTable->rowCount();
	if(inOutRowCount >= tableRowCount) {
		m_stateTable->insertRow(inOutRowCount);
	}

	QTableWidgetItem *keyItem = m_stateTable->item(inOutRowCount, 0);
	if(keyItem) {
		keyItem->setText(key);
	} else {
		keyItem = new QTableWidgetItem;
		keyItem->setText(key);
		Qt::ItemFlags keyFlags = keyItem->flags();
		keyFlags.setFlag(Qt::ItemIsEditable, false);
		keyItem->setFlags(keyFlags);
		m_stateTable->setItem(inOutRowCount, 0, keyItem);
	}

	QTableWidgetItem *valueItem = m_stateTable->item(inOutRowCount, 1);
	if(valueItem) {
		valueItem->setText(value);
	} else {
		valueItem = new QTableWidgetItem;
		valueItem->setText(key);
		Qt::ItemFlags valueFlags = valueItem->flags();
		valueFlags.setFlag(Qt::ItemIsEditable, false);
		valueItem->setFlags(valueFlags);
		m_stateTable->setItem(inOutRowCount, 1, valueItem);
	}

	++inOutRowCount;
}
#endif

}
