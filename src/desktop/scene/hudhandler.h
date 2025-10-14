// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_HUDHANDLER_H
#define DESKTOP_SCENE_HUDHANDLER_H
#include "desktop/scene/hudaction.h"
#include <QObject>
#include <QVector>

namespace drawingboard {
class BaseItem;
class CatchupItem;
class NoticeItem;
class ToggleItem;
}

class HudScene {
public:
	virtual ~HudScene() = default;

	virtual QRectF hudSceneRect() const = 0;

	virtual void hudAddItem(drawingboard::BaseItem *item) = 0;
	virtual void hudRemoveItem(drawingboard::BaseItem *item) = 0;
};

class HudHandler final : public QObject {
	Q_OBJECT
public:
	explicit HudHandler(HudScene *scene, QObject *parent = nullptr);

	bool hasCatchup() const { return m_catchup != nullptr; }

	HudAction checkHover(const QPointF &scenePos);
	void removeHover();

	void setTopOffset(int topOffset);

	bool showTransformNotice(const QString &text);
	bool showLockNotice(const QString &text);
	bool hideLockNotice();
	void showPopupNotice(const QString &text);
	void setToolNotice(const QString &text);
	void setCatchupProgress(int percent);
	void setStreamResetProgress(int percent);
	void setShowToggleItems(bool showToggleItems, bool leftyMode);

	void advanceAnimations(qreal dt);

	void updateItemsPositions();

private:
	static constexpr qreal NOTICE_OFFSET = 16.0;
	static constexpr qreal NOTICE_PERSIST = 1.0;
	static constexpr qreal POPUP_PERSIST = 3.0;

	void updateTransformNoticePosition();
	void updateLockNoticePosition();
	void updateToolNoticePosition();
	void updatePopupNoticePosition();
	void updateCatchupPosition();
	void updateStreamResetNoticePosition();
	void updateToggleItemsPositions();

	static QString getStreamResetProgressText(int percent);

	HudScene *const m_scene;
	qreal m_topOffset = 0.0;
	drawingboard::NoticeItem *m_transformNotice = nullptr;
	drawingboard::NoticeItem *m_lockNotice = nullptr;
	drawingboard::NoticeItem *m_toolNotice = nullptr;
	drawingboard::NoticeItem *m_popupNotice = nullptr;
	drawingboard::CatchupItem *m_catchup = nullptr;
	drawingboard::NoticeItem *m_streamResetNotice = nullptr;
	QVector<drawingboard::ToggleItem *> m_toggleItems;
};

#endif
