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

SelectionSettings::SelectionSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent), m_ui(nullptr)
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

	connect(m_ui->flip, &QAbstractButton::clicked, this, &SelectionSettings::flipSelection);
	connect(m_ui->mirror, SIGNAL(clicked()), this, SLOT(mirrorSelection()));
	connect(m_ui->fittoscreen, SIGNAL(clicked()), this, SLOT(fitToScreen()));
	connect(m_ui->resetsize, SIGNAL(clicked()), this, SLOT(resetSize()));
	connect(m_ui->scale, SIGNAL(clicked()), this, SLOT(scale()));
	connect(m_ui->rotateShear, SIGNAL(clicked()), this, SLOT(rotateShear()));
	connect(m_ui->distort, SIGNAL(clicked()), this, SLOT(distort()));
	connect(controller(), &ToolController::modelChanged, this, &SelectionSettings::modelChanged);

	setControlsEnabled(false);

	return uiwidget;
}

void SelectionSettings::flipSelection()
{
	if(!controller()->model())
		return;

	canvas::Selection *sel = controller()->model()->selection();
	if(sel) {
		cutSelection();
		sel->scale(1, -1);
	}
}

void SelectionSettings::mirrorSelection()
{
	if(!controller()->model())
		return;

	canvas::Selection *sel = controller()->model()->selection();
	if(sel) {
		cutSelection();
		sel->scale(-1, 1);
	}
}

void SelectionSettings::fitToScreen()
{
	Q_ASSERT(m_view);
	if(!controller()->model())
		return;

	canvas::Selection *sel = controller()->model()->selection();
	if(sel) {
		cutSelection();
		const QSizeF size = sel->shape().boundingRect().size();
		const QRectF screenRect = m_view->mapToScene(m_view->rect()).boundingRect();
		const QSizeF screen = screenRect.size() * 0.7;

		if(size.width() > screen.width() || size.height() > screen.height()) {
			const QSizeF newsize = size.scaled(screen, Qt::KeepAspectRatio);
			sel->scale(newsize.width() / size.width(), newsize.height() / size.height());
		}

		if(!sel->boundingRect().intersects(screenRect.toRect())) {
			QPointF offset = screenRect.center() - sel->boundingRect().center();
			sel->translate(offset.toPoint());
		}
	}
}

void SelectionSettings::resetSize()
{
	if(!controller()->model())
		return;

	canvas::Selection *sel = controller()->model()->selection();
	if(sel)
		sel->resetShape();
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

void SelectionSettings::updateSelectionMode(canvas::Selection::AdjustmentMode mode)
{
	if(!controller()->model())
		return;

	canvas::Selection *sel = controller()->model()->selection();
	if(sel && sel->adjustmentMode() != mode)
		sel->setAdjustmentMode(mode);
}

void SelectionSettings::modelChanged(canvas::CanvasModel *model)
{
	if(model) {
		connect(model, &canvas::CanvasModel::selectionChanged, this, &SelectionSettings::selectionChanged);
		selectionChanged(model->selection());
	}
}

void SelectionSettings::selectionChanged(canvas::Selection *selection)
{
	setControlsEnabled(selection && selection->isClosed());
	if(selection) {
		connect(selection, &canvas::Selection::adjustmentModeChanged, this, &SelectionSettings::selectionAdjustmentModeChanged);
		connect(selection, &canvas::Selection::closed, this, &SelectionSettings::selectionClosed);
		selectionAdjustmentModeChanged(selection->adjustmentMode());
	}
}

void SelectionSettings::selectionClosed()
{
	setControlsEnabled(true);
}

void SelectionSettings::cutSelection()
{
	canvas::Selection *sel = controller()->model()->selection();
	const int layer = controller()->activeLayer();

	if(sel && sel->pasteImage().isNull() && !controller()->model()->aclState()->isLayerLocked(layer)) {
		static_cast<tools::SelectionTool*>(controller()->getTool(Tool::SELECTION))->startMove();
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

void SelectionSettings::selectionAdjustmentModeChanged(canvas::Selection::AdjustmentMode mode)
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
		qWarning("SelectionSettings::selectionAdjustmentModeChanged: unknown adjustment mode");
		break;
	}
}

}

