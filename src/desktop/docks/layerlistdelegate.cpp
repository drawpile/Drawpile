// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/layerlistdelegate.h"
#include "desktop/docks/layerlistdock.h"
#include "desktop/main.h"
#include "libclient/canvas/layerlist.h"
#include <QIcon>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QStyle>

namespace docks {

LayerListDelegate::LayerListDelegate(LayerList *dock)
	: QItemDelegate(dock)
	, m_dock(dock)
	, m_visibleIcon(QIcon::fromTheme("layer-visible-on"))
	, m_groupIcon(QIcon::fromTheme("folder"))
	, m_censoredIcon(QIcon(":/icons/censored.svg"))
	, m_hiddenIcon(QIcon::fromTheme("layer-visible-off"))
	, m_groupHiddenIcon(QIcon::fromTheme("drawpile_folderhidden"))
	, m_sketchIcon(QIcon::fromTheme("draw-freehand"))
	, m_fillIcon(QIcon::fromTheme("tag"))
	, m_forbiddenIcon(QIcon::fromTheme("cards-block"))
{
}

void LayerListDelegate::paint(
	QPainter *painter, const QStyleOptionViewItem &option,
	const QModelIndex &index) const
{
	QStyleOptionViewItem opt = setOptions(index, option);
	QRect originalRect = opt.rect;
	QRect textRect = originalRect;
	QRect glyphRect = getOpacityGlyphRect(originalRect);
	drawBackgroundFor(painter, opt, index, glyphRect, 0, -1);

	const canvas::LayerListItem &layer =
		index.data().value<canvas::LayerListItem>();
	drawOpacityGlyph(
		glyphRect, painter, layer.opacity, layer.hidden,
		layer.actuallyCensored(), layer.group);

	if(index.data(canvas::LayerListModel::CheckModeRole).toBool()) {
		int checkState =
			index.data(canvas::LayerListModel::CheckStateRole).toInt();
		if(checkState != int(canvas::LayerListModel::NotApplicable)) {
			QRect checkRect = getCheckRect(originalRect);
			drawBackgroundFor(painter, opt, index, checkRect, 1, 0);
			drawSelectionCheckBox(checkRect, painter, opt, checkState);
			textRect.setRight(checkRect.left());
		}
	} else {
		QRect checkRect = getCheckRect(originalRect);
		drawBackgroundFor(painter, opt, index, checkRect, 1, 0);
		qreal originalOpacity = painter->opacity();
		painter->setOpacity(originalOpacity / 2.0);
		drawSelectionCheckBox(
			checkRect, painter, opt,
			opt.state.testFlag(QStyle::State_Selected) ? Qt::PartiallyChecked
													   : Qt::Unchecked);
		painter->setOpacity(originalOpacity);
		textRect.setRight(checkRect.left());
	}

	if(index.data(canvas::LayerListModel::IsSketchModeRole).toBool()) {
		glyphRect = QRect(
			glyphRect.topRight() +
				QPoint(0, opt.rect.height() / 2 - GLYPH_SIZE / 2),
			QSize(GLYPH_SIZE, GLYPH_SIZE));
		drawBackgroundFor(painter, opt, index, glyphRect, 0, -1);
		drawGlyph(m_sketchIcon, glyphRect, painter);
	}

	if(index.data(canvas::LayerListModel::IsFillSourceRole).toBool()) {
		glyphRect = QRect(
			glyphRect.topRight() +
				QPoint(0, opt.rect.height() / 2 - GLYPH_SIZE / 2),
			QSize(GLYPH_SIZE, GLYPH_SIZE));
		drawBackgroundFor(painter, opt, index, glyphRect, 0, -1);
		drawGlyph(m_fillIcon, glyphRect, painter);
	}

	textRect.setLeft(glyphRect.right());
	opt.rect = originalRect;

	if(index.data(canvas::LayerListModel::IsDefaultRole).toBool()) {
		opt.font.setUnderline(true);
	}
	if(!opt.state.testFlag(QStyle::State_Selected)) {
		drawBackgroundFor(painter, opt, index, textRect, 0, 0);
	}
	drawDisplay(painter, opt, textRect, layer.title);
}

bool LayerListDelegate::editorEvent(
	QEvent *event, QAbstractItemModel *model,
	const QStyleOptionViewItem &option, const QModelIndex &index)
{
	QEvent::Type type = event->type();
	if(type == QEvent::MouseButtonPress) {
		emit interacted();

		const QMouseEvent *me = static_cast<QMouseEvent *>(event);
		m_toggledVisibilityId = 0;
		m_toggledSelectionId = 0;
		if(me->button() == Qt::LeftButton) {
			if(handleClick(me, option, index)) {
				return true;
			}
		}

	} else if(type == QEvent::MouseMove) {
		if(m_toggledVisibilityId != 0) {
			const canvas::LayerListItem &layer =
				index.data().value<canvas::LayerListItem>();
			m_toggledVisibilityId = layer.id;
			if(layer.hidden == m_targetVisibility) {
				emit toggleVisibility(layer.id, m_targetVisibility);
			}
			return true;
		}

		if(m_toggledSelectionId != 0) {
			int layerId = index.data(canvas::LayerListModel::IdRole).toInt();
			if(layerId != m_toggledSelectionId) {
				m_toggledSelectionId = layerId;
				if(index.data(canvas::LayerListModel::CheckModeRole).toBool()) {
					int checkState =
						index.data(canvas::LayerListModel::CheckStateRole)
							.toInt();
					// Only allow toggling of layers and collapsed groups,
					// otherwise the effect of dragging over them is just too
					// crazy with partial and fully-checked states flickering.
					bool shouldToggle =
						hasCheckBox(checkState) &&
						(!index.data(canvas::LayerListModel::IsGroupRole)
							  .toBool() ||
						 !m_dock->isExpanded(index)) &&
						(checkState ==
						 int(canvas::LayerListModel::Unchecked)) ==
							m_targetSelection;
					if(shouldToggle) {
						emit toggleChecked(layerId, m_targetSelection);
					}
				} else if(m_dock->isSelected(layerId) != m_targetSelection) {
					emit toggleSelection(index, m_targetSelection);
				}
			}
			return true;
		}

	} else if(type == QEvent::MouseButtonDblClick) {
		m_toggledVisibilityId = 0;
		m_toggledSelectionId = 0;
		const QMouseEvent *me = static_cast<QMouseEvent *>(event);
		if(me->button() == Qt::LeftButton) {
			if(handleClick(me, option, index)) {
				return true;
			}

			emit editProperties(index);
			return true;
		}
	}

	return QItemDelegate::editorEvent(event, model, option, index);
}

QSize LayerListDelegate::sizeHint(
	const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QSize size = QItemDelegate::sizeHint(option, index);
	QFontMetrics fm(option.font);
	int minheight = qMax(fm.height() * 3 / 2, ICON_SIZE) + 2;
	if(size.height() < minheight) {
		size.setHeight(minheight);
	}
	return size;
}

QRect LayerListDelegate::getOpacityGlyphRect(const QRect &originalRect)
{
	return QRect(
		originalRect.topLeft() +
			QPoint(0, originalRect.height() / 2 - GLYPH_SIZE / 2),
		QSize(GLYPH_SIZE, GLYPH_SIZE));
}

QRect LayerListDelegate::getCheckRect(const QRect &originalRect)
{
	return QRect(
		originalRect.topRight() +
			QPoint(-GLYPH_SIZE, originalRect.height() / 2 - GLYPH_SIZE / 2),
		QSize(GLYPH_SIZE, GLYPH_SIZE));
}

bool LayerListDelegate::hasCheckBox(int checkState)
{
	switch(checkState) {
	case int(canvas::LayerListModel::CheckState::NotApplicable):
	case int(canvas::LayerListModel::CheckState::NotCheckable):
		return false;
	default:
		return true;
	}
}

bool LayerListDelegate::handleClick(
	const QMouseEvent *me, const QStyleOptionViewItem &option,
	const QModelIndex &index)
{
	QPoint pos = me->pos();
	if(getOpacityGlyphRect(option.rect).contains(pos)) {
		const canvas::LayerListItem &layer =
			index.data().value<canvas::LayerListItem>();
		m_toggledVisibilityId = layer.id;
		m_targetVisibility = layer.hidden;
		emit toggleVisibility(layer.id, layer.hidden);
		return true;
	}

	if(getCheckRect(option.rect).contains(pos)) {
		if(index.data(canvas::LayerListModel::CheckModeRole).toBool()) {
			int checkState =
				index.data(canvas::LayerListModel::CheckStateRole).toInt();
			if(hasCheckBox(checkState)) {
				int layerId =
					index.data(canvas::LayerListModel::IdRole).toInt();
				m_toggledSelectionId = layerId;
				m_targetSelection =
					checkState == int(canvas::LayerListModel::Unchecked);
				emit toggleChecked(layerId, m_targetSelection);
				return true;
			}
		} else {
			m_toggledSelectionId =
				index.data(canvas::LayerListModel::IdRole).toInt();
			m_targetSelection = !m_dock->isSelected(m_toggledSelectionId);
			emit toggleSelection(index, m_targetSelection);
			return true;
		}
	}

	return false;
}

void LayerListDelegate::drawBackgroundFor(
	QPainter *painter, QStyleOptionViewItem &opt, const QModelIndex &index,
	const QRect &rect, int leftOffset, int rightOffset) const
{
	opt.rect.setLeft(rect.left() + leftOffset);
	opt.rect.setRight(rect.right() + rightOffset);
	drawBackground(painter, opt, index);
}

void LayerListDelegate::drawOpacityGlyph(
	const QRect &rect, QPainter *painter, float value, bool hidden,
	bool censored, bool group) const
{
	QRect r(
		int(rect.left() + rect.width() / 2 - ICON_SIZE / 2),
		int(rect.top() + rect.height() / 2 - ICON_SIZE / 2), ICON_SIZE,
		ICON_SIZE);

	if(hidden) {
		if(group) {
			m_groupHiddenIcon.paint(painter, r);
		} else {
			m_hiddenIcon.paint(painter, r);
		}
	} else {
		qreal originalOpacity = painter->opacity();
		painter->setOpacity(originalOpacity * value);
		if(censored) {
			m_censoredIcon.paint(painter, r);
		} else if(group) {
			m_groupIcon.paint(painter, r);
		} else {
			m_visibleIcon.paint(painter, r);
		}
		painter->setOpacity(originalOpacity);
	}
}

void LayerListDelegate::drawSelectionCheckBox(
	const QRect &rect, QPainter *painter, const QStyleOptionViewItem &option,
	int checkState) const
{
	if(checkState == int(canvas::LayerListModel::NotCheckable)) {
		QRect r(
			int(rect.left() + rect.width() / 2 - ICON_SIZE / 2),
			int(rect.top() + rect.height() / 2 - ICON_SIZE / 2), ICON_SIZE,
			ICON_SIZE);
		m_forbiddenIcon.paint(painter, r);
	} else {
		const QStyle *style = dpApp().style();
		int width =
			qMin(rect.width(), style->pixelMetric(QStyle::PM_IndicatorWidth));
		int height =
			qMin(rect.height(), style->pixelMetric(QStyle::PM_IndicatorHeight));
		QRect r(
			int(rect.left() + rect.width() / 2 - width / 2),
			int(rect.top() + rect.height() / 2 - height / 2), width, height);
		drawCheck(painter, option, r, Qt::CheckState(checkState));
	}
}

void LayerListDelegate::drawGlyph(
	const QIcon &icon, const QRect &rect, QPainter *painter) const
{
	QRect r(
		int(rect.left() + rect.width() / 2 - ICON_SIZE / 2),
		int(rect.top() + rect.height() / 2 - ICON_SIZE / 2), ICON_SIZE,
		ICON_SIZE);
	icon.paint(painter, r);
}

}
