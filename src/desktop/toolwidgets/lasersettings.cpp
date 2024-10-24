// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/lasersettings.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/tools/laser.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/toolproperties.h"
#include "ui_lasersettings.h"
#include <QLabel>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace tools {

namespace props {
static const ToolProperties::RangedValue<int> persistence{
	QStringLiteral("persistence"), 1, 1, 15},
	color{QStringLiteral("color"), 0, 0, 3};
static const ToolProperties::Value<bool> tracking{
	QStringLiteral("tracking"), true};
}

LaserPointerSettings::LaserPointerSettings(
	ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
{
}

LaserPointerSettings::~LaserPointerSettings()
{
	delete m_ui;
}

bool LaserPointerSettings::isLocked()
{
	return !m_featureAccess || !m_laserTrailsShown;
}

void LaserPointerSettings::pushSettings()
{
	LaserPointer *tool =
		static_cast<LaserPointer *>(controller()->getTool(Tool::LASERPOINTER));
	tool->setPersistence(m_ui->persistence->value());

	QColor c;
	if(m_ui->color0->isChecked()) {
		c = m_ui->color0->color();
	} else if(m_ui->color1->isChecked()) {
		c = m_ui->color1->color();
	} else if(m_ui->color2->isChecked()) {
		c = m_ui->color2->color();
	} else if(m_ui->color3->isChecked()) {
		c = m_ui->color3->color();
	}

	brushes::ActiveBrush b;
	b.setQColor(c);
	controller()->setActiveBrush(b);
}

QWidget *LaserPointerSettings::createUiWidget(QWidget *parent)
{
	m_stack = new QStackedWidget(parent);
	m_stack->setContentsMargins(0, 0, 0, 0);

	QWidget *uiWidget = new QWidget;
	m_ui = new Ui_LaserSettings;
	m_ui->setupUi(uiWidget);
	m_stack->addWidget(uiWidget);

	connect(
		m_ui->trackpointer, &QCheckBox::clicked, this,
		&LaserPointerSettings::togglePointerTracking);
	connect(
		m_ui->persistence, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&LaserPointerSettings::pushSettings);
	connect(
		m_ui->color0, &QAbstractButton::toggled, this,
		&LaserPointerSettings::pushSettings);
	connect(
		m_ui->color1, &QAbstractButton::toggled, this,
		&LaserPointerSettings::pushSettings);
	connect(
		m_ui->color2, &QAbstractButton::toggled, this,
		&LaserPointerSettings::pushSettings);
	connect(
		m_ui->color3, &QAbstractButton::toggled, this,
		&LaserPointerSettings::pushSettings);

	QWidget *disabledWidget = new QWidget;
	QVBoxLayout *disabledLayout = new QVBoxLayout(disabledWidget);

	m_permissionDeniedLabel =
		new QLabel(tr("You don't have permission to use the laser pointer."));
	m_permissionDeniedLabel->setTextFormat(Qt::PlainText);
	m_permissionDeniedLabel->setWordWrap(true);
	disabledLayout->addWidget(m_permissionDeniedLabel);

	m_laserTrailsHiddenLabel =
		new QLabel(QStringLiteral("%1<a href=\"#\">%2</a>")
					   .arg(
						   //: This is part of the sentence "Laser trails are
						   //: hidden. _Show_". The latter is a clickable link.
						   tr("Laser trails are hidden. ").toHtmlEscaped(),
						   //: This is part of the sentence "Laser trails are
						   //: hidden. _Show_". The latter is a clickable link.
						   tr("Show").toHtmlEscaped()));
	m_laserTrailsHiddenLabel->setTextFormat(Qt::RichText);
	m_laserTrailsHiddenLabel->setWordWrap(true);
	disabledLayout->addWidget(m_laserTrailsHiddenLabel);
	connect(
		m_laserTrailsHiddenLabel, &QLabel::linkActivated, this,
		&LaserPointerSettings::showLaserTrailsRequested);

	disabledLayout->addStretch();
	m_stack->addWidget(disabledWidget);

	updateWidgets();
	return m_stack;
}

ToolProperties LaserPointerSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(props::tracking, m_ui->trackpointer->isChecked());
	cfg.setValue(props::persistence, m_ui->persistence->value());

	int color = 0;

	if(m_ui->color1->isChecked()) {
		color = 1;
	} else if(m_ui->color2->isChecked()) {
		color = 2;
	} else if(m_ui->color3->isChecked()) {
		color = 3;
	}
	cfg.setValue(props::color, color);

	return cfg;
}

void LaserPointerSettings::restoreToolSettings(const ToolProperties &cfg)
{
	m_ui->trackpointer->setChecked(cfg.value(props::tracking));
	m_ui->persistence->setValue(cfg.value(props::persistence));

	switch(cfg.value(props::color)) {
	case 0:
		m_ui->color0->setChecked(true);
		break;
	case 1:
		m_ui->color1->setChecked(true);
		break;
	case 2:
		m_ui->color2->setChecked(true);
		break;
	case 3:
		m_ui->color3->setChecked(true);
		break;
	}
}

void LaserPointerSettings::setFeatureAccess(bool featureAccess)
{
	bool pointerTrackingBefore = pointerTracking();
	m_featureAccess = featureAccess;
	bool pointerTrackingAfter = pointerTracking();
	if(pointerTrackingBefore != pointerTrackingAfter &&
	   controller()->activeTool() == Tool::LASERPOINTER) {
		emit pointerTrackingToggled(pointerTrackingAfter);
	}
	updateWidgets();
}

void LaserPointerSettings::setLaserTrailsShown(bool laserTrailsShown)
{
	bool pointerTrackingBefore = pointerTracking();
	m_laserTrailsShown = laserTrailsShown;
	bool pointerTrackingAfter = pointerTracking();
	if(pointerTrackingBefore != pointerTrackingAfter &&
	   controller()->activeTool() == Tool::LASERPOINTER) {
		emit pointerTrackingToggled(pointerTrackingAfter);
	}
	updateWidgets();
}

bool LaserPointerSettings::pointerTracking() const
{
	return m_featureAccess && m_laserTrailsShown &&
		   m_ui->trackpointer->isChecked();
}

void LaserPointerSettings::setForeground(const QColor &color)
{
	m_ui->color0->setColor(color);
	pushSettings();
}

void LaserPointerSettings::quickAdjust1(qreal adjustment)
{
	m_quickAdjust1 += adjustment;
	qreal i;
	qreal f = modf(m_quickAdjust1, &i);
	if(int(i)) {
		m_quickAdjust1 = f;
		m_ui->persistence->setValue(m_ui->persistence->value() + int(i));
	}
}

void LaserPointerSettings::stepAdjust1(bool increase)
{
	QSpinBox *persistence = m_ui->persistence;
	persistence->setValue(stepLogarithmic(
		persistence->minimum(), persistence->maximum(), persistence->value(),
		increase));
}

void LaserPointerSettings::togglePointerTracking(bool checked)
{
	if(m_featureAccess && m_laserTrailsShown) {
		emit pointerTrackingToggled(checked);
	}
}

void LaserPointerSettings::updateWidgets()
{
	if(m_stack) {
		utils::ScopedUpdateDisabler disabler(m_stack);
		m_stack->setCurrentIndex(m_featureAccess && m_laserTrailsShown ? 0 : 1);
		m_permissionDeniedLabel->setVisible(!m_featureAccess);
		m_laserTrailsHiddenLabel->setVisible(!m_laserTrailsShown);
	}
}

}
