// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/baseitem.h"

namespace drawingboard {

void Base::setUpdateSceneOnRefresh(bool updateSceneOnRefresh)
{
	m_updateSceneOnRefresh = updateSceneOnRefresh;
}

BaseItem::BaseItem(QGraphicsItem *parent)
	: QGraphicsItem(parent)
{
}

BaseObject::BaseObject(QGraphicsItem *parent)
	: QGraphicsObject(parent)
{
}

}
