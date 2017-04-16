/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2017 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "selectionsettings.h"
#include "canvas/selection.h"
#include "canvas/canvasmodel.h"
#include "canvas/statetracker.h"
#include "net/client.h"
#include "scene/canvasview.h"
#include "tools/toolcontroller.h"
#include "tools/toolproperties.h"
#include "tools/selection.h"

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

void SelectionSettings::cutSelection()
{
	canvas::Selection *sel = controller()->model()->selection();
	const int layer = controller()->activeLayer();

	if(sel && sel->pasteImage().isNull() && !controller()->model()->stateTracker()->isLayerLocked(layer)) {
		static_cast<tools::SelectionTool*>(controller()->getTool(Tool::SELECTION))->startMove();
	}
}

}

