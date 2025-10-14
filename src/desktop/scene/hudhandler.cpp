// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/hudhandler.h"
#include "desktop/scene/catchupitem.h"
#include "desktop/scene/noticeitem.h"
#include "desktop/scene/toggleitem.h"
#include <QCoreApplication>
#include <QIcon>

using drawingboard::BaseItem;
using drawingboard::CatchupItem;
using drawingboard::NoticeItem;
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
{
	Q_ASSERT(m_scene);
	qRegisterMetaType<HudAction>();
}

HudAction HudHandler::checkHover(const QPointF &scenePos)
{
	HudAction action;
	for(ToggleItem *ti : m_toggleItems) {
		if(ti->checkHover(scenePos, action.wasHovering)) {
			action.type = ti->hudActionType();
		}
	}
	return action;
}

void HudHandler::removeHover()
{
	for(ToggleItem *ti : m_toggleItems) {
		ti->removeHover();
	}
}

void HudHandler::setTopOffset(int topOffset)
{
	if(topOffset != int(m_topOffset)) {
		m_topOffset = qreal(topOffset);
		if(m_transformNotice) {
			updateTransformNoticePosition();
		}
		if(m_lockNotice) {
			updateLockNoticePosition();
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

bool HudHandler::showLockNotice(const QString &text)
{
	if(m_lockNotice) {
		if(!m_lockNotice->setText(text)) {
			return false;
		}
	} else {
		m_lockNotice = new NoticeItem(text);
		m_scene->hudAddItem(m_lockNotice);
	}
	updateLockNoticePosition();
	return true;
}

bool HudHandler::hideLockNotice()
{
	if(m_lockNotice) {
		delete m_lockNotice;
		m_lockNotice = nullptr;
		return true;
	} else {
		return false;
	}
}

void HudHandler::showPopupNotice(const QString &text)
{
	if(!text.isEmpty()) {
		if(m_popupNotice) {
			m_popupNotice->setPersist(POPUP_PERSIST);
			m_popupNotice->setText(text);
		} else {
			m_popupNotice = new NoticeItem(text, POPUP_PERSIST);
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
		m_catchup = new CatchupItem(QCoreApplication::translate(
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
	if(m_lockNotice) {
		updateLockNoticePosition();
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
	updateToggleItemsPositions();
}

void HudHandler::updateTransformNoticePosition()
{
	m_transformNotice->updatePosition(
		m_scene->hudSceneRect().topLeft() +
		QPointF(NOTICE_OFFSET, NOTICE_OFFSET + m_topOffset));
}

void HudHandler::updateLockNoticePosition()
{
	m_lockNotice->updatePosition(
		m_scene->hudSceneRect().topRight() +
		QPointF(
			-m_lockNotice->boundingRect().width() - NOTICE_OFFSET,
			NOTICE_OFFSET + m_topOffset));
}

void HudHandler::updateToolNoticePosition()
{
	QRectF toolNoticeBounds = m_toolNotice->boundingRect();
	m_toolNotice->updatePosition(
		m_scene->hudSceneRect().bottomLeft() +
		QPointF(NOTICE_OFFSET, -toolNoticeBounds.height() - NOTICE_OFFSET));
}

void HudHandler::updatePopupNoticePosition()
{
	QRectF popupNoticeBounds = m_popupNotice->boundingRect();
	QRectF sceneRect = m_scene->hudSceneRect();
	m_popupNotice->updatePosition(
		sceneRect.topLeft() +
		QPointF(
			(sceneRect.width() - popupNoticeBounds.width()) / 2.0,
			NOTICE_OFFSET));
}

void HudHandler::updateCatchupPosition()
{
	QRectF catchupBounds = m_catchup->boundingRect();
	m_catchup->updatePosition(
		m_scene->hudSceneRect().bottomRight() -
		QPointF(
			catchupBounds.width() + NOTICE_OFFSET,
			catchupBounds.height() + NOTICE_OFFSET));
}

void HudHandler::updateStreamResetNoticePosition()
{
	qreal catchupOffset =
		m_catchup ? m_catchup->boundingRect().height() + NOTICE_OFFSET : 0.0;
	QRectF streamResetNoticeBounds = m_streamResetNotice->boundingRect();
	m_streamResetNotice->updatePosition(
		m_scene->hudSceneRect().bottomRight() -
		QPointF(
			streamResetNoticeBounds.width() + NOTICE_OFFSET,
			streamResetNoticeBounds.height() + NOTICE_OFFSET + catchupOffset));
}

void HudHandler::updateToggleItemsPositions()
{
	QRectF sceneRect = m_scene->hudSceneRect();
	for(ToggleItem *ti : m_toggleItems) {
		ti->updateSceneBounds(sceneRect);
	}
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
