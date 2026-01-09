// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/curvepresetdialog.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/kis_curve_widget.h"
#include "libclient/config/config.h"
#include "libclient/tools/toolcontroller.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QtMath>

namespace dialogs {

CurvePresetDialog::CurvePresetDialog(
	const KisCubicCurve &current, int mode, bool pressure, QWidget *parent)
	: QDialog(parent)
{
	setModal(true);

	m_curveWidget = new KisCurveWidget{this};
	m_curveWidget->setReadOnly(true);
	m_curveWidget->setMode(mode);
	m_curveWidget->setFixedSize(300, 300);

	m_presetList = new QListWidget{this};

	QHBoxLayout *buttons = new QHBoxLayout;

	m_useButton = new QPushButton(tr("Use"), this);
	buttons->addWidget(m_useButton);

	QPushButton *cancelButton = new QPushButton(tr("Cancel"), this);
	buttons->addWidget(cancelButton);

	m_saveRenameButton = new QPushButton{tr("Rename"), this};
	buttons->addWidget(m_saveRenameButton);

	m_deleteButton = new QPushButton{tr("Delete"), this};
	buttons->addWidget(m_deleteButton);

	QGridLayout *grid = new QGridLayout{this};
	grid->addWidget(m_curveWidget, 0, 0, 2, 1);
	grid->addWidget(m_presetList, 0, 1);
	grid->addLayout(buttons, 1, 1);

	connect(
		m_presetList, &QListWidget::currentItemChanged, this,
		&CurvePresetDialog::curveSelected);
	connect(
		m_presetList, &QListWidget::itemDoubleClicked, this,
		&CurvePresetDialog::curveDoubleClicked);
	connect(
		m_useButton, &QPushButton::clicked, this, &CurvePresetDialog::accept);
	connect(
		cancelButton, &QPushButton::clicked, this, &CurvePresetDialog::reject);
	connect(
		m_saveRenameButton, &QPushButton::clicked, this,
		&CurvePresetDialog::saveRenameCurve);
	connect(
		m_deleteButton, &QPushButton::clicked, this,
		&CurvePresetDialog::deleteCurve);

	bool linear = mode == KisCurveWidget::MODE_LINEAR ||
				  mode == KisCurveWidget::MODE_LINEAR_SEGMENT;
	loadPresets(current, linear, pressure);
}

CurvePresetDialog::~CurvePresetDialog() {}

KisCubicCurve CurvePresetDialog::curve() const
{
	return m_curveWidget->curve();
}

void CurvePresetDialog::curveSelected(
	QListWidgetItem *current, QListWidgetItem *previous)
{
	Q_UNUSED(previous);
	bool isUnsaved = false;
	if(current) {
		KisCubicCurve curve;
		curve.fromString(current->data(CurveRole).toString());
		m_curveWidget->setCurve(curve);
		switch(current->data(TypeRole).toInt()) {
		case Unsaved:
			m_useButton->setEnabled(false);
			m_saveRenameButton->setEnabled(true);
			m_deleteButton->setEnabled(false);
			isUnsaved = true;
			break;
		case Saved:
			m_useButton->setEnabled(true);
			m_saveRenameButton->setEnabled(true);
			m_deleteButton->setEnabled(true);
			break;
		case Builtin:
			m_useButton->setEnabled(true);
			m_saveRenameButton->setEnabled(false);
			m_deleteButton->setEnabled(false);
			break;
		}
	} else {
		m_useButton->setEnabled(false);
		m_saveRenameButton->setEnabled(false);
		m_deleteButton->setEnabled(false);
	}

	m_saveRenameButton->setText(isUnsaved ? tr("Save") : tr("Rename"));
}

void CurvePresetDialog::curveDoubleClicked(QListWidgetItem *item)
{
	if(item) {
		curveSelected(item, nullptr);
		accept();
	}
}

void CurvePresetDialog::saveRenameCurve()
{
	QListWidgetItem *item = m_presetList->currentItem();
	if(item) {
		int type = item->data(TypeRole).toInt();
		if(type == Unsaved) {
			utils::getInputText(
				this, tr("Save Curve"), tr("Name"), QString(),
				[this, item](const QString &name) {
					QString trimmedName = name.trimmed();
					if(!trimmedName.isEmpty()) {
						item->setIcon(getPresetIcon(Saved));
						item->setText(name.trimmed());
						item->setData(TypeRole, Saved);
						QFont font = item->font();
						font.setItalic(false);
						item->setFont(font);
						savePresets();
						curveSelected(item, nullptr);
					}
				});
		} else if(type == Saved) {
			utils::getInputText(
				this, tr("Rename Curve"), tr("Name"), QString(),
				[this, item](const QString &name) {
					QString trimmedName = name.trimmed();
					if(!trimmedName.isEmpty()) {
						item->setText(name.trimmed());
						savePresets();
					}
				});
		}
	}
}

void CurvePresetDialog::deleteCurve()
{
	QListWidgetItem *item = m_presetList->currentItem();
	if(item && item->data(TypeRole).toInt() == Saved) {
		QMessageBox *box = utils::showQuestion(
			this, tr("Delete Curve"),
			tr("Really delete curve '%1'?").arg(item->text()));
		connect(box, &QMessageBox::accepted, this, [this, item] {
			delete item; // Deleting the item will remove it from the list.
			savePresets();
		});
	}
}

void CurvePresetDialog::loadPresets(
	const KisCubicCurve &current, bool linear, bool pressure)
{
	loadSavedPresets();
	loadFunctionPreset(linear, tr("Smooth Out"), quadraticOut);
	loadFunctionPreset(linear, tr("Smooth In"), quadraticIn);
	loadFunctionPreset(linear, tr("Smooth"), quadraticInOut);
	if(pressure) {
		KisCubicCurve lowPressureCurve;
		lowPressureCurve.fromString(tools::ToolController::lowPressurePenCurve);
		addPreset(
			//: Apple and Xiaomi are brands. This is referring to pressure
			//: curves that make sense for their styluses by default.
			tr("Low-Pressure Stylus (Apple, Xiaomi)"), Builtin,
			lowPressureCurve);

		KisCubicCurve antiStrainCurve;
		antiStrainCurve.fromString(
			QStringLiteral("0,0;0.353014,0.500314;0.849246,1;1,1;"));
		//: This refers to a stylus pressure curve that is steeper than the
		//: default so that you don't have to strain to press the stylus down.
		addPreset(tr("Anti-Strain"), Builtin, antiStrainCurve);
	}
	addPreset(tr("Linear"), Builtin, KisCubicCurve{{{0.0, 0.0}, {1.0, 1.0}}});
	addPreset(tr("Current (unsaved)"), Unsaved, current);
	m_presetList->setCurrentRow(0);
}

void CurvePresetDialog::loadSavedPresets()
{
	QVector<QVariantMap> presets = dpAppConfig()->getCurvesPresets();
	for(const QVariantMap &preset : presets) {
		KisCubicCurve curve;
		curve.fromString(preset.value("curve").toString());
		addPreset(preset.value("name").toString(), Saved, curve);
	}
}

void CurvePresetDialog::loadFunctionPreset(
	bool linear, const QString &name, double (*f)(double))
{
	QList<QPointF> points;
	int npoints = linear ? 10 : 4;
	points.reserve(npoints);
	for(int i = 0; i <= npoints; ++i) {
		double x = 1.0 / double(npoints) * double(i);
		points.append({x, f(x)});
	}
	addPreset(name, Builtin, KisCubicCurve{points});
}

void CurvePresetDialog::addPreset(
	const QString &name, int type, const KisCubicCurve &curve)
{
	QListWidgetItem *item = new QListWidgetItem{getPresetIcon(type), name};
	item->setData(CurveRole, curve.toString());
	item->setData(TypeRole, type);
	if(type == Unsaved) {
		QFont font = item->font();
		font.setItalic(true);
		item->setFont(font);
	}
	m_presetList->insertItem(0, item);
}

QIcon CurvePresetDialog::getPresetIcon(int type)
{
	switch(type) {
	case Saved:
		return QIcon::fromTheme("pathshape");
	case Builtin:
		return QIcon::fromTheme("insert-math-expression");
	default:
		return QIcon{};
	}
}

void CurvePresetDialog::savePresets()
{
	QVector<QVariantMap> presets;
	int count = m_presetList->count();
	for(int i = 0; i < count; ++i) {
		QListWidgetItem *item = m_presetList->item(i);
		if(item->data(TypeRole).toInt() == Saved) {
			presets.append(
				{{"name", item->text()},
				 {"curve", item->data(CurveRole).toString()}});
		}
	}
	dpAppConfig()->setCurvesPresets(presets);
}

double CurvePresetDialog::quadraticIn(double x)
{
	return x * x;
}

double CurvePresetDialog::quadraticOut(double x)
{
	return 1 - (1 - x) * (1 - x);
}

double CurvePresetDialog::quadraticInOut(double x)
{
	return x < 0.5 ? 2.0 * x * x : 1.0 - qPow(-2.0 * x + 2.0, 2.0) / 2.0;
}

}
