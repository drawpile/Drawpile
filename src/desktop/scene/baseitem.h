// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_BASEITEM_H
#define DESKTOP_SCENE_BASEITEM_H
#include <QGraphicsItem>
#include <QGraphicsObject>
#include <QGraphicsScene>
#include <QTransform>

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
		PathPreviewType = QGraphicsItem::UserType + 20,
		TransformType = QGraphicsItem::UserType + 21,
		MaskPreviewType = QGraphicsItem::UserType + 22,
		ColorPickType = QGraphicsItem::UserType + 23,
		AnchorLineItemType = QGraphicsItem::UserType + 24,
	};

	static constexpr qreal Z_USER_MARKER = 999.0;
	static constexpr qreal Z_NOTICE = 9999.0;
	static constexpr qreal Z_CATCHUP = Z_NOTICE;
	static constexpr qreal Z_TOOL_NOTICE = Z_NOTICE + 1.0;
	static constexpr qreal Z_TOGGLE = 99999.0;
	static constexpr qreal Z_COLORPICK = 999998.0;
	static constexpr qreal Z_CURSOR = 999999.0;

	void setUpdateSceneOnRefresh(bool updateSceneOnRefresh);

private:
	void doUpdatePosition(QGraphicsItem *item, const QPointF &pos)
	{
		if(m_updateSceneOnRefresh) {
			item->scene()->update(actualSceneBoundingRect(item));
		}
		item->setPos(pos);
		if(m_updateSceneOnRefresh) {
			item->scene()->update(actualSceneBoundingRect(item));
		}
	}

	void doUpdateRotation(QGraphicsItem *item, qreal rotation)
	{
		maybeDoSceneUpdate(item);
		item->setRotation(rotation);
		maybeDoSceneUpdate(item);
	}

	void doRefresh(QGraphicsItem *item)
	{
		item->update();
		maybeDoSceneUpdate(item);
	}

	void doUpdateVisibility(QGraphicsItem *item, bool visible)
	{
		if(m_internallyVisible != visible) {
			m_internallyVisible = visible;
			item->setVisible(visible);
			maybeDoSceneUpdate(item);
		}
	}

	void maybeDoSceneUpdate(QGraphicsItem *item)
	{
		if(m_updateSceneOnRefresh && m_internallyVisible) {
			item->scene()->update(actualSceneBoundingRect(item));
		}
	}

	static QRectF actualSceneBoundingRect(QGraphicsItem *item)
	{
		if(item->flags().testFlag(QGraphicsItem::ItemIgnoresTransformations)) {
			qreal rotation = item->rotation();
			if(rotation == 0.0) {
				return item->boundingRect().translated(item->scenePos());
			} else {
				QTransform tf;
				QPointF t = item->scenePos();
				tf.translate(t.x(), t.y());
				tf.rotate(rotation);
				return tf.map(item->boundingRect()).boundingRect();
			}
		} else {
			return item->sceneBoundingRect();
		}
	}

	bool m_updateSceneOnRefresh = false;
	bool m_internallyVisible = true;
};

class BaseItem : public QGraphicsItem, public Base {
public:
	void updatePosition(const QPointF &pos) { doUpdatePosition(this, pos); }
	void updateRotation(qreal rotation) { doUpdateRotation(this, rotation); }
	void updateVisibility(bool visible) { doUpdateVisibility(this, visible); }

protected:
	BaseItem(QGraphicsItem *parent);
	void refresh() { doRefresh(this); }
	void refreshGeometry() { prepareGeometryChange(); }
};

class BaseObject : public QGraphicsObject, public Base {
public:
	void updatePosition(const QPointF &pos) { doUpdatePosition(this, pos); }
	void updateRotation(qreal rotation) { doUpdateRotation(this, rotation); }
	void updateVisibility(bool visible) { doUpdateVisibility(this, visible); }

protected:
	BaseObject(QGraphicsItem *parent);
	void refresh() { doRefresh(this); }
	void refreshGeometry() { prepareGeometryChange(); }
};

}

#endif
