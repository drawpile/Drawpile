// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_HUDHANDLER_H
#define DESKTOP_SCENE_HUDHANDLER_H
#include "libclient/view/hudaction.h"
#include <QObject>
#include <QVector>

class QAction;

namespace drawingboard {
class ActionBarItem;
class BaseItem;
class CatchupItem;
class NoticeItem;
class StatusItem;
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
	enum class ActionBar {
		None,
		Selection,
		Transform,
	};

	explicit HudHandler(HudScene *scene, QObject *parent = nullptr);

	ActionBar currentActionBar() { return m_currentActionBar; }
	void setCurrentActionBar(ActionBar actionBar);

	void setActionBarLocation(int location);

	drawingboard::ActionBarItem *selectionActionBar()
	{
		return m_selectionActionBar;
	}

	drawingboard::ActionBarItem *transformActionBar()
	{
		return m_transformActionBar;
	}

	bool hasCatchup() const { return m_catchup != nullptr; }

	HudAction checkHover(const QPointF &scenePos);
	void removeHover();
	void activateHudAction(const HudAction &action, const QPoint &globalPos);

	void setTopOffset(int topOffset);

	bool showTransformNotice(const QString &text);
	bool showLockStatus(const QString &text, const QVector<QAction *> &actions);
	bool hideLockStatus();
	void showPopupNotice(const QString &text);
	void setToolNotice(const QString &text);
	void setCatchupProgress(int percent);
	void setStreamResetProgress(int percent);
	void setShowToggleItems(bool showToggleItems, bool leftyMode);

	void advanceAnimations(qreal dt);

	void updateItemsPositions();

signals:
	void currentActionBarChanged();
	void hudActionActivated(const HudAction &action, const QPoint &globalPos);

private:
	static constexpr qreal NOTICE_OFFSET = 16.0;
	static constexpr qreal NOTICE_PERSIST = 1.0;
	static constexpr qreal POPUP_PERSIST = 3.0;

	QPointF adjustAroundActionBar(const QPointF &pos, int location);

	static QPointF
	adjustByActionBarHeight(const QPointF &pos, int location, qreal height);

	void removeActionBarHover();

	void updateTransformNoticePosition();
	void updateLockStatusPosition();
	void updateToolNoticePosition();
	void updatePopupNoticePosition();
	void updateCatchupPosition();
	void updateStreamResetNoticePosition();
	void updateSelectionActionBarPosition();
	void updateTransformActionBarPosition();
	void updateToggleItemsPositions();

	void refreshApplicationStyle();
	void refreshApplicationFont();

	static QString getStreamResetProgressText(int percent);

	HudScene *const m_scene;
	qreal m_topOffset = 0.0;
	ActionBar m_currentActionBar = ActionBar::None;
	int m_actionBarLocation;
	drawingboard::NoticeItem *m_transformNotice = nullptr;
	drawingboard::StatusItem *m_lockStatus = nullptr;
	drawingboard::NoticeItem *m_toolNotice = nullptr;
	drawingboard::NoticeItem *m_popupNotice = nullptr;
	drawingboard::CatchupItem *m_catchup = nullptr;
	drawingboard::NoticeItem *m_streamResetNotice = nullptr;
	drawingboard::ActionBarItem *m_selectionActionBar = nullptr;
	drawingboard::ActionBarItem *m_transformActionBar = nullptr;
	QVector<drawingboard::ToggleItem *> m_toggleItems;
};

#endif
