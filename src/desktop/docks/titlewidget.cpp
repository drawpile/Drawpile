// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/titlewidget.h"
#include "desktop/docks/dockbase.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QPainter>
#include <QTextOption>
#include <QVBoxLayout>

namespace docks {

namespace {
class DragLabel : public QWidget {
public:
	explicit DragLabel(QWidget *parent = nullptr)
		: QWidget(parent)
	{
		setFixedHeight(16);
	}

	void setText(const QString &text)
	{
		m_text = text;
		update();
	}

protected:
	void paintEvent(QPaintEvent *event) override
	{
		QWidget::paintEvent(event);
		if(!m_text.isEmpty()) {
			QPainter painter(this);
			QFont f = font();
			f.setPixelSize(height() - 4);
			painter.setFont(f);
			painter.setPen(QPen(palette().windowText(), 1.0));
			painter.setBrush(Qt::NoBrush);
			painter.drawText(rect(), Qt::AlignCenter, m_text);
		}
	}

private:
	QString m_text;
};
}

TitleWidget::TitleWidget(QDockWidget *parent)
	: QWidget(parent)
{
	// Retain the title bar's size when it's hidden to avoid jiggering.
	utils::setWidgetRetainSizeWhenHidden(this, true);

	m_dragLayout = new QVBoxLayout(this);
	m_dragLayout->setContentsMargins(2, 2, 1, 2);
	m_dragLayout->setSpacing(4);

	m_layout = new QHBoxLayout;
	m_layout->setSpacing(0);
	m_layout->setContentsMargins(0, 0, 0, 0);
	m_dragLayout->addLayout(m_layout);

	m_button = new widgets::GroupedToolButton{
		widgets::GroupedToolButton::NotGrouped, this};
	m_button->setIcon(QIcon::fromTheme("window_"));
	m_button->setPopupMode(QToolButton::InstantPopup);
	m_layout->addWidget(m_button);

	m_menu = new QMenu(this);
	connect(
		m_menu, &QMenu::aboutToShow, this,
		&TitleWidget::updateContextMenuActions);
	m_button->setMenu(m_menu);

	m_dockAction = m_menu->addAction(tr("Docked"));
	m_dockAction->setCheckable(true);
	connect(
		m_dockAction, &QAction::triggered, this, &TitleWidget::toggleFloating);

	m_dockableAction = m_menu->addAction(tr("Dockable by Dragging"));
	m_dockableAction->setCheckable(true);
	connect(
		m_dockableAction, &QAction::triggered, this,
		&TitleWidget::toggleDockable);

	m_dragSpaceAction = m_menu->addAction(tr("Show Title Bar"));
	m_dragSpaceAction->setStatusTip(tr("Show dock title for easier dragging."));
	m_dragSpaceAction->setToolTip(m_dragSpaceAction->statusTip());
	m_dragSpaceAction->setCheckable(true);
	connect(
		m_dragSpaceAction, &QAction::triggered, this,
		&TitleWidget::setDragSpaceEnabled);

	m_closeAction = m_menu->addAction(tr("Close"));
	connect(m_closeAction, &QAction::triggered, parent, &QDockWidget::close);

	connect(
		parent, &QDockWidget::featuresChanged, this,
		&TitleWidget::onFeaturesChanged);
	connect(
		parent, &QDockWidget::dockLocationChanged, this,
		&TitleWidget::onDockLocationChanged);
	QMainWindow *mainWindow =
		qobject_cast<QMainWindow *>(parent->parentWidget());
	if(mainWindow) {
		onDockLocationChanged(mainWindow->dockWidgetArea(parent));
	}
}

void TitleWidget::addCustomWidget(QWidget *widget, int stretch)
{
	m_layout->insertWidget(m_layout->count() - 1, widget);
	if(stretch > 0) {
		m_layout->setStretchFactor(widget, stretch);
	}
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
	for(QAction *action : actions) {
		if(action) {
			m_menu->addAction(action);
		} else {
			m_menu->addSeparator();
		}
	}
}

void TitleWidget::setKeepButtonSpace(bool keepButtonSpace)
{
	QSizePolicy sizePolicy = m_button->sizePolicy();
	sizePolicy.setRetainSizeWhenHidden(keepButtonSpace);
	m_button->setSizePolicy(sizePolicy);
}

void TitleWidget::setDragSpaceEnabled(bool enabled)
{
	if(enabled && !m_dragLabel) {
		utils::ScopedUpdateDisabler disabler(this);
		DragLabel *dragLabel = new DragLabel;
		QWidget *parent = parentWidget();
		if(parent) {
			DockBase *base = qobject_cast<DockBase *>(parent);
			if(base) {
				dragLabel->setText(base->fullTitle());
			} else {
				dragLabel->setText(parent->windowTitle());
			}
		}
		m_dragLayout->insertWidget(0, dragLabel);
		m_dragLabel = dragLabel;

	} else if(!enabled && m_dragLayout) {
		utils::ScopedUpdateDisabler disabler(this);
		delete m_dragLabel;
		m_dragLabel = nullptr;
	}

	m_dragSpaceAction->setChecked(enabled);
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
	m_closeAction->setEnabled(
		features.testFlag(QDockWidget::DockWidgetClosable));

	if(parent && parent->isFloating()) {
		m_dockAction->setChecked(false);
		m_dockableAction->setEnabled(true);
		m_dockableAction->setChecked(
			parent->allowedAreas() != Qt::NoDockWidgetArea);
	} else {
		m_dockAction->setChecked(true);
		m_dockableAction->setEnabled(false);
		m_dockableAction->setChecked(true);
	}
}

void TitleWidget::onFeaturesChanged(QDockWidget::DockWidgetFeatures features)
{
	m_button->setVisible(features.testFlag(QDockWidget::DockWidgetMovable));
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
