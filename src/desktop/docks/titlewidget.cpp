/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2021-2023 Calle Laakkonen, askmeaboutloom

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

#include "titlewidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QMainWindow>
#include <QMenu>
#include "utils/icon.h"
#include "../../libshared/qtshims.h"

namespace docks {

TitleWidget::TitleWidget(QDockWidget *parent) : QWidget(parent)
{
	// Retain the title bar's size when it's hidden to avoid jiggering.
	QSizePolicy sp = sizePolicy();
	sp.setRetainSizeWhenHidden(true);
	setSizePolicy(sp);

	m_layout = new QHBoxLayout;
	m_layout->setSpacing(0);
	m_layout->setContentsMargins(6, 2, 1, 2);
	setLayout(m_layout);

	m_button = new QToolButton{this};
	m_button->setIcon(icon::fromTheme("window"));
	m_button->setPopupMode(QToolButton::InstantPopup);
	m_layout->addWidget(m_button);

	m_menu = new QMenu{this};
	connect(m_menu, &QMenu::aboutToShow, this, &TitleWidget::updateContextMenuActions);
	m_button->setMenu(m_menu);

	m_dockAction = new QAction{tr("Docked"), m_menu};
	m_dockAction->setCheckable(true);
	connect(m_dockAction, &QAction::triggered, this, &TitleWidget::toggleFloating);
	m_menu->addAction(m_dockAction);

	m_dockableAction = new QAction{tr("Dockable by Dragging"), m_menu};
	m_dockableAction->setCheckable(true);
	connect(m_dockableAction, &QAction::triggered, this, &TitleWidget::toggleDockable);
	m_menu->addAction(m_dockableAction);

	m_closeAction = new QAction{tr("Close"), m_menu};
	connect(m_closeAction, &QAction::triggered, parent, &QDockWidget::close);
	m_menu->addAction(m_closeAction);

	connect(parent, &QDockWidget::dockLocationChanged, this, &TitleWidget::onDockLocationChanged);
	QMainWindow *mainWindow = qobject_cast<QMainWindow *>(parent->parentWidget());
	if(mainWindow) {
		onDockLocationChanged(mainWindow->dockWidgetArea(parent));
	}
}

void TitleWidget::addCustomWidget(QWidget *widget, bool stretch)
{
	m_layout->insertWidget(m_layout->count() - 1, widget);
	if(stretch)
		m_layout->setStretchFactor(widget, 1);
}


void TitleWidget::addSpace(int space)
{
	m_layout->insertSpacing(m_layout->count() - 1, space);
}

void TitleWidget::addStretch(int stretch)
{
	m_layout->insertStretch(m_layout->count() - 1, stretch);
}

void TitleWidget::addCenteringSpacer()
{
	m_layout->insertStretch(0);
	m_layout->insertSpacing(0, m_button->width());
	m_layout->insertStretch(m_layout->count() - 1);
}

void TitleWidget::addGlobalDockActions(const QList<QAction *> &actions)
{
	m_menu->addSeparator();
	m_menu->addActions(actions);
}

void TitleWidget::toggleFloating()
{
	QDockWidget *parent = qobject_cast<QDockWidget *>(parentWidget());
	if(parent) {
		if(parent->isFloating()) {
			parent->setAllowedAreas(Qt::AllDockWidgetAreas);
			parent->setFloating(false);
		} else {
			parent->setFloating(true);
		}
	}
}

void TitleWidget::toggleDockable()
{
	QDockWidget *parent = qobject_cast<QDockWidget *>(parentWidget());
	if(parent && parent->isFloating()) {
		if(parent->allowedAreas() == Qt::NoDockWidgetArea) {
			parent->setAllowedAreas(Qt::AllDockWidgetAreas);
		} else {
			parent->setAllowedAreas(Qt::NoDockWidgetArea);
		}
	}
}

void TitleWidget::updateContextMenuActions()
{
	QDockWidget *parent = qobject_cast<QDockWidget *>(parentWidget());

	QDockWidget::DockWidgetFeatures features =
		parent ? parent->features() : QDockWidget::NoDockWidgetFeatures;
	m_dockAction->setEnabled(features.testFlag(QDockWidget::DockWidgetMovable));
	m_closeAction->setEnabled(features.testFlag(QDockWidget::DockWidgetClosable));

	if(parent && parent->isFloating()) {
		m_dockAction->setChecked(false);
		m_dockableAction->setEnabled(true);
		m_dockableAction->setChecked(parent->allowedAreas() != Qt::NoDockWidgetArea);
	} else {
		m_dockAction->setChecked(true);
		m_dockableAction->setEnabled(false);
		m_dockableAction->setChecked(true);
	}
}

void TitleWidget::onDockLocationChanged(Qt::DockWidgetArea area)
{
	if(area != Qt::NoDockWidgetArea) {
		QDockWidget *parent = qobject_cast<QDockWidget *>(parentWidget());
		if(parent) {
			parent->setAllowedAreas(Qt::AllDockWidgetAreas);
		}
	}
}

}
