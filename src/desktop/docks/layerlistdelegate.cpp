// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/docks/layerlistdelegate.h"
#include "libclient/canvas/layerlist.h"

#include <QDebug>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QLineEdit>

namespace docks {

static const QSize ICON_SIZE { 16, 16 };

LayerListDelegate::LayerListDelegate(QObject *parent)
	: QItemDelegate(parent),
	  m_visibleIcon(QIcon::fromTheme("layer-visible-on")),
	  m_groupIcon(QIcon::fromTheme("folder")),
	  m_censoredIcon(QIcon(":/icons/censored.svg")),
	  m_hiddenIcon(QIcon::fromTheme("layer-visible-off")),
	  m_groupHiddenIcon(QIcon::fromTheme("drawpile_folderhidden"))
{
}

void LayerListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QStyleOptionViewItem opt = setOptions(index, option);
	painter->save();

	const canvas::LayerListItem &layer = index.data().value<canvas::LayerListItem>();

	if(index.data(canvas::LayerListModel::IsLockedRole).toBool() ||
	   index.data(canvas::LayerListModel::IsHiddenInFrameRole).toBool() ||
	   index.data(canvas::LayerListModel::IsHiddenInTreeRole).toBool()) {
		opt.state &= ~QStyle::State_Enabled;
	}

	drawBackground(painter, opt, index);

	QRect textrect = opt.rect;

	// Draw layer opacity glyph
	QRect stylerect(opt.rect.topLeft() + QPoint(0, opt.rect.height()/2-12), QSize(24,24));
	drawOpacityGlyph(stylerect, painter, layer.opacity, layer.hidden, layer.censored, layer.group);

	// Draw layer name
	textrect.setLeft(stylerect.right());

	if(index.data(canvas::LayerListModel::IsDefaultRole).toBool()) {
		opt.font.setUnderline(true);
	}

	drawDisplay(painter, opt, textrect, layer.title);

	painter->restore();
}

bool LayerListDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
	QEvent::Type type = event->type();
	if(type == QEvent::MouseButtonPress) {
		const canvas::LayerListItem &layer = index.data().value<canvas::LayerListItem>();
		const QMouseEvent *me = static_cast<QMouseEvent*>(event);

		if(me->button() == Qt::LeftButton) {
			const QRect glyphrect {
				option.rect.topLeft() + QPoint(0, option.rect.height()/2-12),
				QSize(24,24)
			};

			if(glyphrect.contains(me->pos())) {
				// Clicked on opacity glyph: toggle visibility
				emit toggleVisibility(layer.id, layer.hidden);
				m_justToggledVisibility = true;
				return true;
			}
		}
		m_justToggledVisibility = false;

	} else if (type == QEvent::MouseMove) {
		// Mouse movements with the button held down cause the layer to be
		// selected, which is immensely annoying when you're just trying to
		// toggle visibility. So if the last click was a visibility toggle, we
		// eat the event to prevent that from happening.
		if(m_justToggledVisibility) {
			return true;
		}

	} else if(type == QEvent::MouseButtonDblClick) {
		m_justToggledVisibility = false;
		const QMouseEvent *me = static_cast<QMouseEvent*>(event);
		if(me->button() == Qt::LeftButton) {
			const QRect glyphrect {
				option.rect.topLeft() + QPoint(0, option.rect.height()/2-12),
				QSize(24,24)
			};

			if(!glyphrect.contains(me->pos()))
				emit editProperties(index);

			return true;
		}
	}

	return QItemDelegate::editorEvent(event, model, option, index);
}

QSize LayerListDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const
{
	QSize size = QItemDelegate::sizeHint(option, index);
	QFontMetrics fm(option.font);
	int minheight = qMax(fm.height() * 3 / 2, ICON_SIZE.height()) + 2;
	if(size.height() < minheight)
		size.setHeight(minheight);
	return size;
}

void LayerListDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem & option, const QModelIndex &) const
{
	const int btnwidth = 24;

	static_cast<QLineEdit*>(editor)->setFrame(true);
	editor->setGeometry(option.rect.adjusted(btnwidth, 0, -btnwidth, 0));
}

void LayerListDelegate::drawOpacityGlyph(const QRectF& rect, QPainter *painter, float value, bool hidden, bool censored, bool group) const
{
	const QRect r {
		int(rect.left() + rect.width() / 2 - ICON_SIZE.width()/2),
		int(rect.top() + rect.height() / 2 - ICON_SIZE.height()/2),
		ICON_SIZE.width(),
		ICON_SIZE.height(),
	};

	if(hidden) {
		if(group) {
			m_groupHiddenIcon.paint(painter, r);
		} else {
			m_hiddenIcon.paint(painter, r);
		}

	} else {
		painter->save();
		painter->setOpacity(value);
		if(censored)
			m_censoredIcon.paint(painter, r);
		else if(group)
			m_groupIcon.paint(painter, r);
		else
			m_visibleIcon.paint(painter, r);
		painter->restore();
	}
}

}
