// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CURVEPRESETDIALOG_H
#define CURVEPRESETDIALOG_H
#include "libclient/utils/kis_cubic_curve.h"
#include <QDialog>

class KisCurveWidget;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSettings;

namespace dialogs {

class CurvePresetDialog final : public QDialog {
	Q_OBJECT
public:
	CurvePresetDialog(
		const KisCubicCurve &current, bool linear, QWidget *parent = nullptr);
	~CurvePresetDialog() override;

	KisCubicCurve curve() const;

private slots:
	void curveSelected(QListWidgetItem *current, QListWidgetItem *previous);
	void curveDoubleClicked(QListWidgetItem *item);
	void saveRenameCurve();
	void deleteCurve();

private:
	enum { CurveRole = Qt::UserRole, TypeRole };
	enum { Unsaved, Saved, Builtin };

	void loadPresets(const KisCubicCurve &current, bool linear);
	void loadSavedPresets();
	void convertInputPresetsToCurvePresets(QSettings &cfg);
	static bool isLinearCurve(const QString &curveString);
	void
	loadFunctionPreset(bool linear, const QString &name, double (*f)(double));
	void addPreset(const QString &name, int type, const KisCubicCurve &curve);
	static QIcon getPresetIcon(int type);

	void savePresets();

	static double quadraticIn(double x);
	static double quadraticOut(double x);
	static double quadraticInOut(double x);

	KisCurveWidget *m_curveWidget;
	QListWidget *m_presetList;
	QPushButton *m_useButton;
	QPushButton *m_saveRenameButton;
	QPushButton *m_deleteButton;
};

}

#endif
