// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/docks/layerlistdelegate.h"
#include "desktop/main.h"
#include "libclient/canvas/layerlist.h"
#include <QDebug>
#include <QIcon>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>

namespace docks {

LayerListDelegate::LayerListDelegate(QObject *parent)
	: QItemDelegate(parent)
	, m_visibleIcon(QIcon::fromTheme("layer-visible-on"))
	, m_groupIcon(QIcon::fromTheme("folder"))
	, m_censoredIcon(QIcon(":/icons/censored.svg"))
	, m_hiddenIcon(QIcon::fromTheme("layer-visible-off"))
	, m_groupHiddenIcon(QIcon::fromTheme("drawpile_folderhidden"))
	, m_fillIcon(QIcon::fromTheme("tag"))
	, m_forbiddenIcon(QIcon::fromTheme("cards-block"))
{
}

void LayerListDelegate::paint(
	QPainter *painter, const QStyleOptionViewItem &option,
	const QModelIndex &index) const
{
	QStyleOptionViewItem opt = setOptions(index, option);
	painter->save();

	const canvas::LayerListItem &layer =
		index.data().value<canvas::LayerListItem>();

	if(index.data(canvas::LayerListModel::IsLockedRole).toBool() ||
	   index.data(canvas::LayerListModel::IsHiddenInFrameRole).toBool() ||
	   index.data(canvas::LayerListModel::IsHiddenInTreeRole).toBool() ||
	   index.data(canvas::LayerListModel::IsCensoredInTreeRole).toBool()) {
		opt.state &= ~QStyle::State_Enabled;
	}

	drawBackground(painter, opt, index);

	QRect textRect = opt.rect;

	QRect opacityGlyphRect = getOpacityGlyphRect(option);
	drawOpacityGlyph(
		opacityGlyphRect, painter, layer.opacity, layer.hidden,
		layer.actuallyCensored(), layer.group);

	int checkState = index.data(canvas::LayerListModel::CheckStateRole).toInt();
	if(checkState != int(canvas::LayerListModel::NotApplicable)) {
		QRect checkRect = getCheckRect(opt);
		drawSelectionCheckBox(checkRect, painter, opt, checkState);
		textRect.setRight(checkRect.left());
	}

	if(index.data(canvas::LayerListModel::IsFillSourceRole).toBool()) {
		QRect fillGlyphRect(
			opacityGlyphRect.topRight() +
				QPoint(0, opt.rect.height() / 2 - GLYPH_SIZE / 2),
			QSize(GLYPH_SIZE, GLYPH_SIZE));
		drawFillGlyph(fillGlyphRect, painter);
		textRect.setLeft(fillGlyphRect.right());
	} else {
		textRect.setLeft(opacityGlyphRect.right());
	}

	if(index.data(canvas::LayerListModel::IsDefaultRole).toBool()) {
		opt.font.setUnderline(true);
	}
	drawDisplay(painter, opt, textRect, layer.title);

	painter->restore();
}

bool LayerListDelegate::editorEvent(
	QEvent *event, QAbstractItemModel *model,
	const QStyleOptionViewItem &option, const QModelIndex &index)
{
	QEvent::Type type = event->type();
	if(type == QEvent::MouseButtonPress) {
		emit interacted();

		const QMouseEvent *me = static_cast<QMouseEvent *>(event);

		if(me->button() == Qt::LeftButton) {
			QPoint pos = me->pos();

			if(getOpacityGlyphRect(option).contains(pos)) {
				// Clicked on opacity glyph: toggle visibility
				const canvas::LayerListItem &layer =
					index.data().value<canvas::LayerListItem>();
				emit toggleVisibility(layer.id, layer.hidden);
				m_justToggledVisibility = true;
				return true;
			}

			if(getCheckRect(option).contains(pos)) {
				int checkState =
					index.data(canvas::LayerListModel::CheckStateRole).toInt();
				if(hasCheckBox(checkState)) {
					emit toggleChecked(
						index.data(canvas::LayerListModel::IdRole).toInt(),
						checkState == int(canvas::LayerListModel::Unchecked));
					return true;
				}
			}
		}
		m_justToggledVisibility = false;

	} else if(type == QEvent::MouseMove) {
		// Mouse movements with the button held down cause the layer to be
		// selected, which is immensely annoying when you're just trying to
		// toggle visibility. So if the last click was a visibility toggle, we
		// eat the event to prevent that from happening.
		if(m_justToggledVisibility) {
			return true;
		}

	} else if(type == QEvent::MouseButtonDblClick) {
		m_justToggledVisibility = false;
		const QMouseEvent *me = static_cast<QMouseEvent *>(event);
		if(me->button() == Qt::LeftButton) {
			QPoint pos = me->pos();
			if(!getOpacityGlyphRect(option).contains(pos) &&
			   !(getCheckRect(option).contains(pos) &&
				 hasCheckBox(index.data(canvas::LayerListModel::CheckStateRole)
								 .toInt()))) {
				emit editProperties(index);
			}

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

QRect LayerListDelegate::getOpacityGlyphRect(const QStyleOptionViewItem &opt)
{
	return QRect(
		opt.rect.topLeft() + QPoint(0, opt.rect.height() / 2 - GLYPH_SIZE / 2),
		QSize(GLYPH_SIZE, GLYPH_SIZE));
}

QRect LayerListDelegate::getCheckRect(const QStyleOptionViewItem &opt)
{
	return QRect(
		opt.rect.topRight() +
			QPoint(-GLYPH_SIZE, opt.rect.height() / 2 - GLYPH_SIZE / 2),
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
		painter->save();
		painter->setOpacity(value);
		if(censored) {
			m_censoredIcon.paint(painter, r);
		} else if(group) {
			m_groupIcon.paint(painter, r);
		} else {
			m_visibleIcon.paint(painter, r);
		}
		painter->restore();
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

void LayerListDelegate::drawFillGlyph(
	const QRect &rect, QPainter *painter) const
{
	QRect r(
		int(rect.left() + rect.width() / 2 - ICON_SIZE / 2),
		int(rect.top() + rect.height() / 2 - ICON_SIZE / 2), ICON_SIZE,
		ICON_SIZE);
	m_fillIcon.paint(painter, r);
}

}
