// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_BASEITEM_H
#define DESKTOP_SCENE_BASEITEM_H
#include <QGraphicsItem>
#include <QGraphicsObject>
#include <QGraphicsScene>

namespace drawingboard {

// An item that can refresh itself even when it's using view::CanvasScene, since
// that's not attached to a QGraphicsView. Use refresh() instead of update() and
// refreshGeometry() instead of prepareGeometryChange().
class Base {
	friend class BaseItem;
	friend class BaseObject;

public:
	enum {
		AnnotationType = QGraphicsItem::UserType + 10,
		SelectionType = QGraphicsItem::UserType + 11,
		UserMarkerType = QGraphicsItem::UserType + 12,
		LaserTrailType = QGraphicsItem::UserType + 13,
		NoticeType = QGraphicsItem::UserType + 15,
		ToggleType = QGraphicsItem::UserType + 16,
		CatchupType = QGraphicsItem::UserType + 17,
		OutlineType = QGraphicsItem::UserType + 18,
		CursorType = QGraphicsItem::UserType + 19,
	};

	void setUpdateSceneOnRefresh(bool updateSceneOnRefresh);

private:
	void doRefresh(QGraphicsItem *item)
	{
		item->update();
		if(m_updateSceneOnRefresh) {
			item->scene()->update(item->sceneBoundingRect());
		}
	}

	bool m_updateSceneOnRefresh = false;
};

class BaseItem : public QGraphicsItem, public Base {
protected:
	BaseItem(QGraphicsItem *parent);
	void refresh() { doRefresh(this); }
	void refreshGeometry() { prepareGeometryChange(); }
};

class BaseObject : public QGraphicsObject, public Base {
protected:
	BaseObject(QGraphicsItem *parent);
	void refresh() { doRefresh(this); }
	void refreshGeometry() { prepareGeometryChange(); }
};

}

#endif
