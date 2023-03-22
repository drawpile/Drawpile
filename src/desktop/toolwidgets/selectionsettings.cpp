// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/toolwidgets/selectionsettings.h"
#include "libclient/canvas/selection.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/net/client.h"
#include "desktop/scene/canvasview.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/toolproperties.h"
#include "libclient/tools/selection.h"

#include "ui_selectsettings.h"

namespace tools {

namespace props {
static const ToolProperties::RangedValue<int> interpolation{
	QStringLiteral("interpolation"), 1, 0, 1};
}

SelectionSettings::SelectionSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings{ctrl, parent}
	, m_ui{nullptr}
{
}

SelectionSettings::~SelectionSettings()
{
	delete m_ui;
}


QWidget *SelectionSettings::createUiWidget(QWidget *parent)
{
	QWidget *uiwidget = new QWidget(parent);
	m_ui = new Ui_SelectionSettings;
	m_ui->setupUi(uiwidget);

	m_ui->interpolationCombo->addItem(
		tr("Nearest"), DP_MSG_MOVE_REGION_MODE_NEAREST);
	m_ui->interpolationCombo->addItem(
		tr("Bilinear"), DP_MSG_MOVE_REGION_MODE_BILINEAR);
	connect(
		m_ui->interpolationCombo,
		QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&SelectionSettings::pushSettings);

	connect(
		m_ui->flip, &QAbstractButton::pressed, this,
		&SelectionSettings::flipSelection);
	connect(
		m_ui->mirror, &QAbstractButton::pressed, this,
		&SelectionSettings::mirrorSelection);
	connect(
		m_ui->fittoscreen, &QAbstractButton::pressed, this,
		&SelectionSettings::fitToScreen);
	connect(
		m_ui->resetsize, &QAbstractButton::pressed, this,
		&SelectionSettings::resetSize);
	connect(
		m_ui->scale, &QAbstractButton::pressed, this,
		&SelectionSettings::scale);
	connect(
		m_ui->rotateShear, &QAbstractButton::pressed, this,
		&SelectionSettings::rotateShear);
	connect(
		m_ui->distort, &QAbstractButton::pressed, this,
		&SelectionSettings::distort);
	connect(
		controller(), &ToolController::modelChanged, this,
		&SelectionSettings::modelChanged);

	setControlsEnabled(false);

	return uiwidget;
}

void SelectionSettings::flipSelection()
{
	canvas::CanvasModel *model = controller()->model();
	if(!model) {
		return;
	}

	canvas::Selection *sel = model->selection();
	if(sel) {
		cutSelection();
		sel->scale(1, -1);
	}
}

void SelectionSettings::mirrorSelection()
{
	canvas::CanvasModel *model = controller()->model();
	if(!model) {
		return;
	}

	canvas::Selection *sel = model->selection();
	if(sel) {
		cutSelection();
		sel->scale(-1, 1);
	}
}

void SelectionSettings::fitToScreen()
{
	Q_ASSERT(m_view);
	canvas::CanvasModel *model = controller()->model();
	if(!model) {
		return;
	}

	canvas::Selection *sel = model->selection();
	if(sel) {
		cutSelection();
		const QSizeF size = sel->shape().boundingRect().size();
		const QRectF screenRect =
			m_view->mapToScene(m_view->rect()).boundingRect();
		const QSizeF screen = screenRect.size() * 0.7;

		if(size.width() > screen.width() || size.height() > screen.height()) {
			const QSizeF newsize = size.scaled(screen, Qt::KeepAspectRatio);
			sel->scale(
				newsize.width() / size.width(),
				newsize.height() / size.height());
		}

		if(!sel->boundingRect().intersects(screenRect.toRect())) {
			QPointF offset = screenRect.center() - sel->boundingRect().center();
			sel->translate(offset.toPoint());
		}
	}
}

void SelectionSettings::resetSize()
{
	canvas::CanvasModel *model = controller()->model();
	if(!model) {
		return;
	}

	canvas::Selection *sel = model->selection();
	if(sel) {
		sel->resetShape();
	}
}

void SelectionSettings::scale()
{
	updateSelectionMode(canvas::Selection::AdjustmentMode::Scale);
}

void SelectionSettings::rotateShear()
{
	updateSelectionMode(canvas::Selection::AdjustmentMode::Rotate);
}

void SelectionSettings::distort()
{
	updateSelectionMode(canvas::Selection::AdjustmentMode::Distort);
}

tools::RectangleSelection *SelectionSettings::getRectangleSelectionTool()
{
	return static_cast<tools::RectangleSelection *>(
		controller()->getTool(Tool::SELECTION));
}

tools::PolygonSelection *SelectionSettings::getPolygonSelectionTool()
{
	return static_cast<tools::PolygonSelection *>(
		controller()->getTool(Tool::POLYGONSELECTION));
}

void SelectionSettings::updateSelectionMode(
	canvas::Selection::AdjustmentMode mode)
{
	canvas::CanvasModel *model = controller()->model();
	if(!model) {
		return;
	}

	canvas::Selection *sel = model->selection();
	if(sel && sel->adjustmentMode() != mode) {
		sel->setAdjustmentMode(mode);
	}
}

void SelectionSettings::modelChanged(canvas::CanvasModel *model)
{
	if(model) {
		connect(
			model, &canvas::CanvasModel::selectionChanged, this,
			&SelectionSettings::selectionChanged);
		selectionChanged(model->selection());
	}
}

void SelectionSettings::selectionChanged(canvas::Selection *selection)
{
	setControlsEnabled(selection && selection->isClosed());
	if(selection) {
		connect(
			selection, &canvas::Selection::adjustmentModeChanged, this,
			&SelectionSettings::selectionAdjustmentModeChanged);
		connect(
			selection, &canvas::Selection::closed, this,
			&SelectionSettings::selectionClosed);
		selectionAdjustmentModeChanged(selection->adjustmentMode());
	}
}

void SelectionSettings::selectionClosed()
{
	setControlsEnabled(true);
}

void SelectionSettings::cutSelection()
{
	tools::ToolController *ctrl = controller();
	canvas::CanvasModel *model = ctrl->model();
	canvas::Selection *sel = model->selection();
	int layer = ctrl->activeLayer();
	bool canCut = sel && sel->pasteImage().isNull() &&
				  !model->aclState()->isLayerLocked(layer);
	if(canCut) {
		getRectangleSelectionTool()->startMove();
	}
}

void SelectionSettings::setControlsEnabled(bool enabled)
{
	m_ui->flip->setEnabled(enabled);
	m_ui->mirror->setEnabled(enabled);
	m_ui->fittoscreen->setEnabled(enabled);
	m_ui->resetsize->setEnabled(enabled);
	m_ui->scale->setEnabled(enabled);
	m_ui->rotateShear->setEnabled(enabled);
	m_ui->distort->setEnabled(enabled);
	if(!enabled) {
		m_ui->scale->setChecked(true);
	}
}

ToolProperties SelectionSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(
		props::interpolation, m_ui->interpolationCombo->currentData().toInt());
	return cfg;
}

void SelectionSettings::restoreToolSettings(const ToolProperties &cfg)
{
	int interpolation = cfg.value(props::interpolation);
	for(int i = 0; i < m_ui->interpolationCombo->count(); ++i) {
		if(m_ui->interpolationCombo->itemData(i).toInt() == interpolation) {
			m_ui->interpolationCombo->setCurrentIndex(i);
			break;
		}
	}
	pushSettings();
}

void SelectionSettings::pushSettings()
{
	int interpolation = m_ui->interpolationCombo->currentData().toInt();
	getRectangleSelectionTool()->setInterpolation(interpolation);
	getPolygonSelectionTool()->setInterpolation(interpolation);
}

void SelectionSettings::selectionAdjustmentModeChanged(
	canvas::Selection::AdjustmentMode mode)
{
	switch(mode) {
	case canvas::Selection::AdjustmentMode::Scale:
		m_ui->scale->setChecked(true);
		break;
	case canvas::Selection::AdjustmentMode::Rotate:
		m_ui->rotateShear->setChecked(true);
		break;
	case canvas::Selection::AdjustmentMode::Distort:
		m_ui->distort->setChecked(true);
		break;
	default:
		qWarning("SelectionSettings::selectionAdjustmentModeChanged: unknown "
				 "adjustment mode");
		break;
	}
}

}
