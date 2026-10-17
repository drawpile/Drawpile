// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/cropsettings.h"
#include "desktop/dialogs/colordialog.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/noscroll.h"
#include "libclient/config/config.h"
#include "libclient/tools/crop.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/view/enums.h"
#include <QActionGroup>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QWidget>

namespace tools {

namespace props {
static const ToolProperties::Value<bool> grow{QStringLiteral("grow"), false};
}

CropSettings::CropSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
{
}

void CropSettings::pushSettings()
{
	CropTool::Properties props;
	props.grow = !m_constrainToCanvasBox->isChecked();
	controller()->cropTool()->setProperties(props);
}

ToolProperties CropSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(props::grow, !m_constrainToCanvasBox->isChecked());
	return cfg;
}

void CropSettings::restoreToolSettings(const ToolProperties &cfg)
{
	utils::setButtonCheckedSignalsBlocked(
		m_constrainToCanvasBox, !cfg.value(props::grow));
}

QWidget *CropSettings::createUiWidget(QWidget *parent)
{
	m_headerWidget = new QWidget(parent);
	m_headerGroup = new QButtonGroup(m_headerWidget);

	QHBoxLayout *headerLayout = new QHBoxLayout(m_headerWidget);
	headerLayout->setContentsMargins(0, 0, 0, 0);
	headerLayout->setSpacing(0);

	widgets::GroupedToolButton *headerMenuButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped);
	headerLayout->addWidget(headerMenuButton);
	headerMenuButton->setIcon(
		QIcon::fromTheme(QStringLiteral("application-menu")));
	headerMenuButton->setPopupMode(QToolButton::InstantPopup);

	QMenu *headerMenu = new QMenu(headerMenuButton);
	headerMenuButton->setMenu(headerMenu);

	QAction *setCropColorAction = headerMenu->addAction(tr("Set shade color…"));
	connect(
		setCropColorAction, &QAction::triggered, this,
		&CropSettings::showCropColorDialog);

	headerMenu->addSection(tr("Guides"));

	QAction *guidesNoneAction = headerMenu->addAction(tr("None", "guides"));
	guidesNoneAction->setProperty(
		"drawpile_guides", int(view::CropGuides::None));
	guidesNoneAction->setCheckable(true);
	guidesNoneAction->setChecked(true);

	QAction *guidesThirdsAction = headerMenu->addAction(tr("Thirds", "guides"));
	guidesThirdsAction->setProperty(
		"drawpile_guides", int(view::CropGuides::Thirds));
	guidesThirdsAction->setCheckable(true);

	QAction *guidesFifthsAction = headerMenu->addAction(tr("Fifths", "guides"));
	guidesFifthsAction->setProperty(
		"drawpile_guides", int(view::CropGuides::Fifths));
	guidesFifthsAction->setCheckable(true);

	QAction *guidesPlusAction =
		headerMenu->addAction(tr("Crosshair", "guides"));
	guidesPlusAction->setProperty(
		"drawpile_guides", int(view::CropGuides::Plus));
	guidesPlusAction->setCheckable(true);

	m_guidesGroup = new QActionGroup(this);
	m_guidesGroup->addAction(guidesNoneAction);
	m_guidesGroup->addAction(guidesThirdsAction);
	m_guidesGroup->addAction(guidesFifthsAction);
	m_guidesGroup->addAction(guidesPlusAction);
	connect(
		m_guidesGroup, &QActionGroup::triggered, this,
		&CropSettings::updateCropGuides);

	headerLayout->addStretch(1);

	widgets::GroupedToolButton *replaceButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	replaceButton->setIcon(
		QIcon::fromTheme(QStringLiteral("drawpile_selection_replace")));
	replaceButton->setStatusTip(
		QCoreApplication::translate(
			"tools::SelectionSettings", "Replace selection"));
	replaceButton->setToolTip(replaceButton->statusTip());
	replaceButton->setCheckable(true);
	replaceButton->setChecked(true);
	headerLayout->addWidget(replaceButton, 3);
	m_headerGroup->addButton(replaceButton, DP_MSG_SELECTION_PUT_OP_REPLACE);

	widgets::GroupedToolButton *uniteButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	uniteButton->setIcon(
		QIcon::fromTheme(QStringLiteral("drawpile_selection_unite")));
	uniteButton->setStatusTip(
		QCoreApplication::translate(
			"tools::SelectionSettings", "Add to selection"));
	uniteButton->setToolTip(uniteButton->statusTip());
	uniteButton->setCheckable(true);
	headerLayout->addWidget(uniteButton, 3);
	m_headerGroup->addButton(uniteButton, DP_MSG_SELECTION_PUT_OP_UNITE);

	widgets::GroupedToolButton *intersectButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupCenter);
	intersectButton->setIcon(
		QIcon::fromTheme(QStringLiteral("drawpile_selection_intersect")));
	intersectButton->setStatusTip(
		QCoreApplication::translate(
			"tools::SelectionSettings", "Intersect with selection"));
	intersectButton->setToolTip(intersectButton->statusTip());
	intersectButton->setCheckable(true);
	headerLayout->addWidget(intersectButton, 3);
	m_headerGroup->addButton(
		intersectButton, DP_MSG_SELECTION_PUT_OP_INTERSECT);

	widgets::GroupedToolButton *m_excludeButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_excludeButton->setIcon(
		QIcon::fromTheme(QStringLiteral("drawpile_selection_exclude")));
	m_excludeButton->setStatusTip(
		QCoreApplication::translate(
			"tools::SelectionSettings", "Remove from selection"));
	m_excludeButton->setToolTip(m_excludeButton->statusTip());
	m_excludeButton->setCheckable(true);
	headerLayout->addWidget(m_excludeButton, 3);
	m_headerGroup->addButton(m_excludeButton, DP_MSG_SELECTION_PUT_OP_EXCLUDE);

	headerLayout->addStretch(1);

	QWidget *widget = new QWidget(parent);
	QFormLayout *form = new QFormLayout(widget);
	form->setContentsMargins(3, 3, 3, 3);
	form->setSpacing(3);

	QHBoxLayout *positionLayout = new QHBoxLayout;
	positionLayout->setContentsMargins(0, 0, 0, 0);
	positionLayout->setSpacing(3);
	form->addRow(tr("Position:"), positionLayout);

	m_xSpinner = new widgets::NoScrollKisSliderSpinBox;
	m_xSpinner->setIndeterminate(true);
	positionLayout->addWidget(m_xSpinner, 1);
	connect(
		m_xSpinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&CropSettings::pushCropArea);

	m_ySpinner = new widgets::NoScrollKisSliderSpinBox;
	m_ySpinner->setIndeterminate(true);
	positionLayout->addWidget(m_ySpinner, 1);
	connect(
		m_ySpinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged), this,
		&CropSettings::pushCropArea);

	m_centerButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped);
	m_centerButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	m_centerButton->setIcon(
		QIcon::fromTheme(QStringLiteral("drawpile_center")));
	m_centerButton->setToolTip(
		QCoreApplication::translate("tools::TransformSettings", "Center"));
	m_centerButton->setStatusTip(m_centerButton->toolTip());
	m_centerButton->setCheckable(true);
	positionLayout->addWidget(m_centerButton);

	QHBoxLayout *sizeLayout = new QHBoxLayout;
	sizeLayout->setContentsMargins(0, 0, 0, 0);
	sizeLayout->setSpacing(3);
	form->addRow(tr("Size:"), sizeLayout);

	m_widthSpinner = new widgets::NoScrollKisSliderSpinBox;
	m_widthSpinner->setIndeterminate(true);
	sizeLayout->addWidget(m_widthSpinner, 1);
	connect(
		m_widthSpinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &CropSettings::pushCropArea);

	m_heightSpinner = new widgets::NoScrollKisSliderSpinBox;
	m_heightSpinner->setIndeterminate(true);
	sizeLayout->addWidget(m_heightSpinner, 1);
	connect(
		m_heightSpinner, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
		this, &CropSettings::pushCropArea);

	m_constrainButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::NotGrouped);
	m_constrainButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	m_constrainButton->setIcon(
		QIcon::fromTheme(QStringLiteral("zoom-original")));
	m_constrainButton->setToolTip(
		QCoreApplication::translate("tools::TransformSettings", "Constrain"));
	m_constrainButton->setStatusTip(m_constrainButton->toolTip());
	m_constrainButton->setCheckable(true);
	sizeLayout->addWidget(m_constrainButton);

	m_constrainToCanvasBox = new QCheckBox(tr("Constrain to canvas"));
	m_constrainToCanvasBox->setToolTip(
		tr("Keep the crop area on the canvas. Disable to allow growing."));
	m_constrainToCanvasBox->setStatusTip(m_constrainToCanvasBox->toolTip());
	form->addRow(m_constrainToCanvasBox);
	connect(
		m_constrainToCanvasBox, &QCheckBox::clicked, this,
		&CropSettings::pushSettings);

	m_cancelButton = new QPushButton(
		widget->style()->standardIcon(QStyle::SP_DialogCancelButton),
		QCoreApplication::translate("QPlatformTheme", "Cancel"));
	m_cancelButton->setStatusTip(tr("Discard the liquified transform"));
	m_cancelButton->setToolTip(m_cancelButton->statusTip());
	m_cancelButton->setEnabled(false);
	connect(
		m_cancelButton, &QPushButton::clicked, controller(),
		&ToolController::cancelMultipartDrawing);
	form->addRow(m_cancelButton);

	config::Config *cfg = dpAppConfig();
	CFG_BIND_SET(cfg, CropGuides, this, CropSettings::setCropGuides);

	connect(
		controller(), &ToolController::cropAreaChanged, this,
		&CropSettings::setCropArea);
	setCropArea(QRect());

	return widget;
}

void CropSettings::showCropColorDialog()
{
	QString objectName = QStringLiteral("cropcolordialog");

	color_widgets::ColorDialog *dlg = findChild<color_widgets::ColorDialog *>(
		objectName, Qt::FindDirectChildrenOnly);
	if(dlg) {
		dlg->activateWindow();
		dlg->raise();
	} else {
		config::Config *cfg = dpAppConfig();
		dlg =
			dialogs::newDeleteOnCloseColorDialog(cfg->getCropColor(), getUi());
		dialogs::setColorDialogResetColor(
			dlg, config::Config::defaultCropColor());
		dlg->setAlphaEnabled(true);
		utils::showWindow(dlg);
		connect(
			dlg, &color_widgets::ColorDialog::colorSelected, cfg,
			&config::Config::setCropColor);
	}
}

void CropSettings::setCropGuides(int cropGuides)
{
	for(QAction *action : m_guidesGroup->actions()) {
		if(action->property("drawpile_guides").toInt() == cropGuides) {
			action->setChecked(true);
			break;
		}
	}
}

void CropSettings::updateCropGuides(QAction *action)
{
	config::Config *cfg = dpAppConfig();
	cfg->setCropGuides(action->property("drawpile_guides").toInt());
}

void CropSettings::setCropArea(QRect cropArea)
{
	bool enabled = !cropArea.isEmpty();
	m_cancelButton->setEnabled(enabled);
	if(enabled) {
		setSpinner(
			m_xSpinner, true, cropArea.x(), POSITION_SPINNER_MIN,
			POSITION_SPINNER_MAX);
		setSpinner(
			m_ySpinner, true, cropArea.y(), POSITION_SPINNER_MIN,
			POSITION_SPINNER_MAX);
		setSpinner(
			m_widthSpinner, true, cropArea.width(), SIZE_SPINNER_MIN,
			SIZE_SPINNER_MAX);
		setSpinner(
			m_heightSpinner, true, cropArea.height(), SIZE_SPINNER_MIN,
			SIZE_SPINNER_MAX);
	} else {
		setSpinner(m_xSpinner, false, 0, 0, 0);
		setSpinner(m_ySpinner, false, 0, 0, 0);
		setSpinner(m_widthSpinner, false, 0, 0, 0);
		setSpinner(m_heightSpinner, false, 0, 0, 0);
	}
}

void CropSettings::pushCropArea()
{
	controller()->cropTool()->setCropArea(QRect(
		m_xSpinner->value(), m_ySpinner->value(), m_widthSpinner->value(),
		m_heightSpinner->value()));
}

void CropSettings::setSpinner(
	KisSliderSpinBox *spinner, bool enabled, int value, int min, int max)
{
	spinner->setEnabled(enabled);
	QSignalBlocker blocker(spinner);
	spinner->setRange(min, max);
	spinner->setValue(value);
}

}
