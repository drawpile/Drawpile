// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_INPUTDEBUGDIALOG_H
#define DESKTOP_DIALOGS_INPUTDEBUGDIALOG_H
#include <QCoreApplication>
#include <QDialog>

#if defined(Q_OS_ANDROID) &&                                                   \
	defined(KRITA_QATTRIBUTE_ANDROID_DEBUG_INPUT_EVENTS)
#	define DRAWPILE_INPUT_DEBUG_DIALOG_ANDROID 1
#else
#	undef DRAWPILE_INPUT_DEBUG_DIALOG_ANDROID
#endif

class QLabel;
class QTableWidget;
class QTableWidgetItem;

namespace dialogs {

class InputDebugDialog final : public QDialog {
	Q_OBJECT
public:
	explicit InputDebugDialog(QWidget *parent = nullptr);

#if DRAWPILE_INPUT_DEBUG_DIALOG_ANDROID
	~InputDebugDialog() override;
#endif

private:
#if DRAWPILE_INPUT_DEBUG_DIALOG_ANDROID
	void handleDebugEvent(const QVariantHash &eventData);

	void addStateRowString(
		const QVariantHash &h, const QString &key, int &inOutRowCount);

	void addStateRowPointer(
		const QVariantHash &h, int pointerIndex, const QString &pointerKey,
		int &inOutRowCount);

	void addStateRowAxis(
		const QVariantHash &h, int pointerIndex, const QString &axis,
		bool withDegrees, int &inOutRowCount);

	void
	addStateRow(const QString &key, const QString &value, int &inOutRowCount);

	QTableWidget *m_stateTable;
#else
	QLabel *m_unsupportedLabel;
#endif
};

}

#endif
