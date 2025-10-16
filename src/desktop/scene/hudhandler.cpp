// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/hudhandler.h"
#include "desktop/main.h"
#include "desktop/scene/actionbaritem.h"
#include "desktop/scene/catchupitem.h"
#include "desktop/scene/noticeitem.h"
#include "desktop/scene/statusitem.h"
#include "desktop/scene/toggleitem.h"
#include <QAction>
#include <QCoreApplication>
#include <QIcon>
#include <functional>

using drawingboard::ActionBarItem;
using drawingboard::BaseItem;
using drawingboard::CatchupItem;
using drawingboard::NoticeItem;
using drawingboard::StatusItem;
using drawingboard::ToggleItem;


namespace {

static void addOrEditToggleItem(
	QVector<drawingboard::ToggleItem *> &toggleItems, bool haveToggleItems,
	int index, HudAction::Type type, Qt::Alignment side, double fromTop)
{
	if(haveToggleItems) {
		toggleItems[index]->setHudActionType(type);
	} else {
		toggleItems.append(new ToggleItem(type, side, fromTop));
	}
}

}


HudHandler::HudHandler(HudScene *scene, QObject *parent)
	: QObject(parent)
	, m_scene(scene)
	, m_actionBarLocation(int(ActionBarItem::Location::BottomCenter))
{
	Q_ASSERT(m_scene);
	qRegisterMetaType<HudAction>();

	DrawpileApp &app = dpApp();
	const QStyle *style = dpApp().style();
	QFont font = app.font();

	m_lockStatus = new StatusItem(style, font);
	m_lockStatus->updateVisibility(false);
	m_scene->hudAddItem(m_lockStatus);
	connect(
		m_lockStatus->overflowMenu(), &QMenu::aboutToShow, this,
		std::bind(&StatusItem::setMenuShown, m_lockStatus, true));
	connect(
		m_lockStatus->overflowMenu(), &QMenu::aboutToHide, this,
		std::bind(&StatusItem::setMenuShown, m_lockStatus, false));

	QAction *overflowAction = new QAction(
		QIcon::fromTheme(QStringLiteral("drawpile_ellipsis_vertical")),
		tr("More…"), this);
	m_selectionActionBar = new ActionBarItem(
		QStringLiteral("- %1 -").arg(tr("Selection")), style, font,
		overflowAction);
	m_transformActionBar = new ActionBarItem(
		QStringLiteral("- %1 -").arg(tr("Transform")), style, font,
		overflowAction);
	m_selectionActionBar->updateVisibility(false);
	m_transformActionBar->updateVisibility(false);
	m_scene->hudAddItem(m_selectionActionBar);
	m_scene->hudAddItem(m_transformActionBar);
	connect(
		m_selectionActionBar->overflowMenu(), &QMenu::aboutToShow, this,
		std::bind(&ActionBarItem::setMenuShown, m_selectionActionBar, true));
	connect(
		m_transformActionBar->overflowMenu(), &QMenu::aboutToShow, this,
		std::bind(&ActionBarItem::setMenuShown, m_transformActionBar, true));
	connect(
		m_selectionActionBar->overflowMenu(), &QMenu::aboutToHide, this,
		std::bind(&ActionBarItem::setMenuShown, m_selectionActionBar, false));
	connect(
		m_transformActionBar->overflowMenu(), &QMenu::aboutToHide, this,
		std::bind(&ActionBarItem::setMenuShown, m_transformActionBar, false));
	connect(
		&app, &DrawpileApp::refreshApplicationStyleRequested, this,
		&HudHandler::refreshApplicationStyle, Qt::QueuedConnection);
	connect(
		&app, &DrawpileApp::refreshApplicationFontRequested, this,
		&HudHandler::refreshApplicationFont, Qt::QueuedConnection);
}

void HudHandler::setCurrentActionBar(ActionBar actionBar)
{
	if(actionBar != m_currentActionBar) {
		switch(m_currentActionBar) {
		case ActionBar::None:
			break;
		case ActionBar::Selection:
			m_selectionActionBar->updateVisibility(false);
			m_selectionActionBar->setMenuShown(false);
			m_selectionActionBar->removeHover();
			break;
		case ActionBar::Transform:
			m_transformActionBar->updateVisibility(false);
			m_selectionActionBar->setMenuShown(false);
			m_transformActionBar->removeHover();
			break;
		}

		m_currentActionBar = actionBar;
		switch(actionBar) {
		case ActionBar::None:
			break;
		case ActionBar::Selection:
			m_selectionActionBar->updateVisibility(true);
			break;
		case ActionBar::Transform:
			m_transformActionBar->updateVisibility(true);
			break;
		}

		updateItemsPositions();
		emit currentActionBarChanged();
	}
}

void HudHandler::setActionBarLocation(int location)
{
	switch(location) {
	case int(ActionBarItem::Location::TopLeft):
	case int(ActionBarItem::Location::TopCenter):
	case int(ActionBarItem::Location::TopRight):
	case int(ActionBarItem::Location::BottomLeft):
	case int(ActionBarItem::Location::BottomCenter):
	case int(ActionBarItem::Location::BottomRight):
		if(location != m_actionBarLocation) {
			m_actionBarLocation = location;
			updateItemsPositions();
		}
		break;
	default:
		qWarning("setActionBarLocation: unknown location %d", location);
		break;
	}
}

HudAction HudHandler::checkHover(const QPointF &scenePos)
{
	HudAction action;

	switch(m_currentActionBar) {
	case ActionBar::None:
		break;
	case ActionBar::Selection:
		m_selectionActionBar->checkHover(scenePos, action);
		break;
	case ActionBar::Transform:
		m_transformActionBar->checkHover(scenePos, action);
		break;
	}

	for(ToggleItem *ti : m_toggleItems) {
		if(ti->checkHover(scenePos, action.wasHovering)) {
			action.type = ti->hudActionType();
		}
	}

	if(m_lockStatus->isInternallyVisible()) {
		m_lockStatus->checkHover(scenePos, action);
	}

	return action;
}

void HudHandler::removeHover()
{
	removeActionBarHover();
	for(ToggleItem *ti : m_toggleItems) {
		ti->removeHover();
	}
	if(m_lockStatus->isInternallyVisible()) {
		m_lockStatus->removeHover();
	}
}

void HudHandler::activateHudAction(
	const HudAction &action, const QPoint &globalPos)
{
	// Avoid the action bar flickering from having its hover removed and then
	// right afterwards turning opaque again from the menu being shown.
	if(action.type == HudAction::Type::TriggerMenu) {
		if(action.menu == m_selectionActionBar->overflowMenu()) {
			m_selectionActionBar->setMenuShown(true);
		} else if(action.menu == m_transformActionBar->overflowMenu()) {
			m_transformActionBar->setMenuShown(true);
		} else if(action.menu == m_lockStatus->overflowMenu()) {
			m_lockStatus->setMenuShown(true);
		}
	}
	emit hudActionActivated(action, globalPos);
}

void HudHandler::setTopOffset(int topOffset)
{
	if(topOffset != int(m_topOffset)) {
		m_topOffset = qreal(topOffset);
		if(m_transformNotice) {
			updateTransformNoticePosition();
		}
		if(m_lockStatus->isInternallyVisible()) {
			updateLockStatusPosition();
		}
		if(m_toolNotice) {
			updateToolNoticePosition();
		}
		if(m_popupNotice) {
			updatePopupNoticePosition();
		}
	}
}

bool HudHandler::showTransformNotice(const QString &text)
{
	if(m_transformNotice) {
		bool opacityChanged = m_transformNotice->setPersist(NOTICE_PERSIST);
		if(!m_transformNotice->setText(text)) {
			return opacityChanged;
		}
	} else {
		m_transformNotice = new NoticeItem(text, NOTICE_PERSIST);
		m_scene->hudAddItem(m_transformNotice);
	}
	updateTransformNoticePosition();
	return true;
}

bool HudHandler::showLockStatus(
	const QString &text, const QVector<QAction *> &actions)
{
	bool changed = m_lockStatus->setStatus(text, actions);
	if(!m_lockStatus->isInternallyVisible()) {
		m_lockStatus->updateVisibility(true);
		updateLockStatusPosition();
	}
	return changed;
}

bool HudHandler::hideLockStatus()
{
	if(m_lockStatus->isInternallyVisible()) {
		m_lockStatus->updateVisibility(false);
		m_lockStatus->clearStatus();
		m_lockStatus->removeHover();
		return true;
	} else {
		return false;
	}
}

void HudHandler::showPopupNotice(const QString &text)
{
	if(!text.isEmpty()) {
		qreal persist = (qreal(text.count('\n')) + 1.0) * POPUP_PERSIST;
		if(m_popupNotice) {
			m_popupNotice->setPersist(persist);
			m_popupNotice->setText(text);
		} else {
			m_popupNotice = new NoticeItem(text, persist);
			m_popupNotice->setAlignment(Qt::AlignCenter);
			m_scene->hudAddItem(m_popupNotice);
		}
		updatePopupNoticePosition();
	}
}

void HudHandler::setToolNotice(const QString &text)
{
	if(text.isEmpty()) {
		if(m_toolNotice) {
			delete m_toolNotice;
			m_toolNotice = nullptr;
		}
	} else {
		if(m_toolNotice) {
			if(m_toolNotice->setText(text)) {
				updateToolNoticePosition();
			}
		} else {
			m_toolNotice = new NoticeItem(text);
			m_toolNotice->setZValue(BaseItem::Z_TOOL_NOTICE);
			m_scene->hudAddItem(m_toolNotice);
			updateToolNoticePosition();
		}
	}
}

void HudHandler::setCatchupProgress(int percent)
{
	if(!m_catchup) {
		m_catchup = new CatchupItem(
			QCoreApplication::translate(
				"view::CanvasScene", "Restoring canvas…"));
		m_scene->hudAddItem(m_catchup);
	}
	m_catchup->setCatchupProgress(percent);
	updateCatchupPosition();
}

void HudHandler::setStreamResetProgress(int percent)
{
	if(percent > 100) {
		if(m_streamResetNotice) {
			m_streamResetNotice->setText(getStreamResetProgressText(percent));
			if(m_streamResetNotice->persist() < 0.0) {
				m_streamResetNotice->setPersist(NOTICE_PERSIST);
			}
		}
	} else {
		if(m_streamResetNotice) {
			m_streamResetNotice->setText(getStreamResetProgressText(percent));
			m_streamResetNotice->setPersist(-1.0);
		} else {
			m_streamResetNotice =
				new NoticeItem(getStreamResetProgressText(percent));
			m_scene->hudAddItem(m_streamResetNotice);
		}
		updateStreamResetNoticePosition();
	}
}

void HudHandler::setShowToggleItems(bool showToggleItems, bool leftyMode)
{
	bool haveToggleItems = !m_toggleItems.isEmpty();
	if(showToggleItems) {
		struct ToggleItemParams {
			HudAction::Type type;
			Qt::Alignment side;
			double fromTop;
		};
		ToggleItemParams params[] = {
			{
				HudAction::Type::ToggleBrush,
				Qt::AlignLeft,
				1.0 / 3.0,
			},
			{
				HudAction::Type::ToggleTimeline,
				Qt::AlignLeft,
				2.0 / 3.0,
			},
			{
				HudAction::Type::ToggleLayer,
				Qt::AlignRight,
				1.0 / 3.0,
			},
			{
				HudAction::Type::ToggleChat,
				Qt::AlignRight,
				2.0 / 3.0,
			},
		};
		if(leftyMode) {
			std::swap(params[0].type, params[1].type);
			std::swap(params[2].type, params[3].type);
		}

		int index = 0;
		for(const ToggleItemParams &tip : params) {
			addOrEditToggleItem(
				m_toggleItems, haveToggleItems, index++, tip.type, tip.side,
				tip.fromTop);
		}

		if(!haveToggleItems) {
			for(ToggleItem *ti : m_toggleItems) {
				m_scene->hudAddItem(ti);
			}
			updateToggleItemsPositions();
		}

	} else if(!showToggleItems && haveToggleItems) {
		for(ToggleItem *ti : m_toggleItems) {
			m_scene->hudRemoveItem(ti);
		}
		m_toggleItems.clear();
	}
}

void HudHandler::advanceAnimations(qreal dt)
{
	if(m_transformNotice && !m_transformNotice->animationStep(dt)) {
		delete m_transformNotice;
		m_transformNotice = nullptr;
	}

	if(m_popupNotice && !m_popupNotice->animationStep(dt)) {
		delete m_popupNotice;
		m_popupNotice = nullptr;
	}

	if(m_catchup && !m_catchup->animationStep(dt)) {
		delete m_catchup;
		m_catchup = nullptr;
	}

	if(m_streamResetNotice && !m_streamResetNotice->animationStep(dt)) {
		delete m_streamResetNotice;
		m_streamResetNotice = nullptr;
	}
}

void HudHandler::updateItemsPositions()
{
	if(m_transformNotice) {
		updateTransformNoticePosition();
	}
	if(m_lockStatus->isInternallyVisible()) {
		updateLockStatusPosition();
	}
	if(m_toolNotice) {
		updateToolNoticePosition();
	}
	if(m_popupNotice) {
		updatePopupNoticePosition();
	}
	if(m_catchup) {
		updateCatchupPosition();
	}
	if(m_streamResetNotice) {
		updateStreamResetNoticePosition();
	}

	switch(m_currentActionBar) {
	case ActionBar::None:
		break;
	case ActionBar::Selection:
		updateSelectionActionBarPosition();
		break;
	case ActionBar::Transform:
		updateTransformActionBarPosition();
		break;
	}

	updateToggleItemsPositions();
}

QPointF HudHandler::adjustAroundActionBar(const QPointF &pos, int location)
{
	if(location == m_actionBarLocation) {
		switch(m_currentActionBar) {
		case ActionBar::None:
			break;
		case ActionBar::Selection:
			return adjustByActionBarHeight(
				pos, location, m_selectionActionBar->boundingRect().height());
		case ActionBar::Transform:
			return adjustByActionBarHeight(
				pos, location, m_transformActionBar->boundingRect().height());
		}
	}
	return pos;
}

QPointF HudHandler::adjustByActionBarHeight(
	const QPointF &pos, int location, qreal height)
{
	qreal y = pos.y();
	if(ActionBarItem::isLocationAtTop(ActionBarItem::Location(location))) {
		y += height;
	} else {
		y -= height;
	}
	return QPointF(pos.x(), y);
}

void HudHandler::removeActionBarHover()
{
	switch(m_currentActionBar) {
	case ActionBar::None:
		break;
	case ActionBar::Selection:
		m_selectionActionBar->removeHover();
		break;
	case ActionBar::Transform:
		m_transformActionBar->removeHover();
		break;
	}
}

void HudHandler::updateTransformNoticePosition()
{
	QPointF pos = m_scene->hudSceneRect().topLeft() +
				  QPointF(NOTICE_OFFSET, NOTICE_OFFSET + m_topOffset);
	m_transformNotice->updatePosition(
		adjustAroundActionBar(pos, int(ActionBarItem::Location::TopLeft)));
}

void HudHandler::updateLockStatusPosition()
{
	QPointF pos = m_scene->hudSceneRect().topRight() +
				  QPointF(-NOTICE_OFFSET, NOTICE_OFFSET + m_topOffset);
	m_lockStatus->updatePosition(
		adjustAroundActionBar(pos, int(ActionBarItem::Location::TopRight)));
}

void HudHandler::updateToolNoticePosition()
{
	QRectF toolNoticeBounds = m_toolNotice->boundingRect();
	QPointF pos =
		m_scene->hudSceneRect().bottomLeft() +
		QPointF(NOTICE_OFFSET, -toolNoticeBounds.height() - NOTICE_OFFSET);
	m_toolNotice->updatePosition(
		adjustAroundActionBar(pos, int(ActionBarItem::Location::BottomLeft)));
}

void HudHandler::updatePopupNoticePosition()
{
	QRectF popupNoticeBounds = m_popupNotice->boundingRect();
	QRectF sceneRect = m_scene->hudSceneRect();
	QPointF pos = sceneRect.topLeft() +
				  QPointF(
					  (sceneRect.width() - popupNoticeBounds.width()) / 2.0,
					  NOTICE_OFFSET);
	m_popupNotice->updatePosition(
		adjustAroundActionBar(pos, int(ActionBarItem::Location::TopCenter)));
}

void HudHandler::updateCatchupPosition()
{
	QRectF catchupBounds = m_catchup->boundingRect();
	QPointF pos = m_scene->hudSceneRect().bottomRight() -
				  QPointF(
					  catchupBounds.width() + NOTICE_OFFSET,
					  catchupBounds.height() + NOTICE_OFFSET);
	m_catchup->updatePosition(
		adjustAroundActionBar(pos, int(ActionBarItem::Location::BottomRight)));
}

void HudHandler::updateStreamResetNoticePosition()
{
	qreal catchupOffset =
		m_catchup ? m_catchup->boundingRect().height() + NOTICE_OFFSET : 0.0;
	QRectF streamResetNoticeBounds = m_streamResetNotice->boundingRect();
	QPointF pos =
		m_scene->hudSceneRect().bottomRight() -
		QPointF(
			streamResetNoticeBounds.width() + NOTICE_OFFSET,
			streamResetNoticeBounds.height() + NOTICE_OFFSET + catchupOffset);
	m_streamResetNotice->updatePosition(
		adjustAroundActionBar(pos, int(ActionBarItem::Location::BottomRight)));
}

void HudHandler::updateSelectionActionBarPosition()
{
	m_selectionActionBar->updateLocation(
		ActionBarItem::Location(m_actionBarLocation), m_scene->hudSceneRect(),
		m_topOffset);
}

void HudHandler::updateTransformActionBarPosition()
{
	m_transformActionBar->updateLocation(
		ActionBarItem::Location(m_actionBarLocation), m_scene->hudSceneRect(),
		m_topOffset);
}

void HudHandler::updateToggleItemsPositions()
{
	QRectF sceneRect = m_scene->hudSceneRect();
	for(ToggleItem *ti : m_toggleItems) {
		ti->updateSceneBounds(sceneRect);
	}
}

void HudHandler::refreshApplicationStyle()
{
	const QStyle *style = dpApp().style();
	m_lockStatus->setStyle(style);
	m_selectionActionBar->setStyle(style);
	m_transformActionBar->setStyle(style);
}

void HudHandler::refreshApplicationFont()
{
	QFont font = dpApp().font();
	m_lockStatus->setFont(font);
	m_selectionActionBar->setFont(font);
	m_transformActionBar->setFont(font);
}

QString HudHandler::getStreamResetProgressText(int percent)
{
	if(percent < 0) {
		return QCoreApplication::translate(
			"view::CanvasScene", "Compressing canvas…");
	} else {
		return QCoreApplication::translate(
				   "view::CanvasScene", "Uploading canvas %1%")
			.arg(qBound(0, percent, 100));
	}
}
