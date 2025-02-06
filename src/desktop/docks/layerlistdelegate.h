// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DOCKS_LAYERLISTDELEGATE_H
#define DESKTOP_DOCKS_LAYERLISTDELEGATE_H
#include <QAbstractListModel>
#include <QIcon>
#include <QItemDelegate>

namespace docks {

class LayerList;

/**
 * \brief A custom item delegate for displaying layer names and editing layer
 * settings.
 */
class LayerListDelegate final : public QItemDelegate {
	Q_OBJECT
public:
	LayerListDelegate(LayerList *layerlist = nullptr);

	void paint(
		QPainter *painter, const QStyleOptionViewItem &option,
		const QModelIndex &index) const override;

	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index)
		const override;

	bool editorEvent(
		QEvent *event, QAbstractItemModel *model,
		const QStyleOptionViewItem &option, const QModelIndex &index) override;

	bool shouldDisableDrag() const
	{
		return m_toggledSelectionId != 0 || m_toggledVisibilityId != 0;
	}

	void clearDrag()
	{
		m_toggledSelectionId = 0;
		m_toggledVisibilityId = 0;
	}

signals:
	void interacted();
	void toggleVisibility(int layerId, bool visible);
	void toggleSelection(const QModelIndex &index);
	void toggleChecked(int layerId, bool checked);
	void editProperties(const QModelIndex &index);

private:
	static constexpr int GLYPH_SIZE = 24;
	static constexpr int ICON_SIZE = 16;

	static QRect getOpacityGlyphRect(const QRect &originalRect);
	static QRect getCheckRect(const QRect &originalRect);
	static bool hasCheckBox(int checkState);

	bool handleClick(
		const QMouseEvent *me, const QStyleOptionViewItem &option,
		const QModelIndex &index);

	void drawBackgroundFor(
		QPainter *painter, QStyleOptionViewItem &opt, const QModelIndex &index,
		const QRect &rect, int leftOffset, int rightOffset) const;

	void drawOpacityGlyph(
		const QRect &rect, QPainter *painter, float value, bool hidden,
		bool censored, bool group) const;

	void drawSelectionCheckBox(
		const QRect &rect, QPainter *painter,
		const QStyleOptionViewItem &option, int checkState) const;

	void
	drawGlyph(const QIcon &icon, const QRect &rect, QPainter *painter) const;

	LayerList *m_dock;
	QIcon m_visibleIcon;
	QIcon m_groupIcon;
	QIcon m_censoredIcon;
	QIcon m_hiddenIcon;
	QIcon m_groupHiddenIcon;
	QIcon m_sketchIcon;
	QIcon m_fillIcon;
	QIcon m_forbiddenIcon;
	int m_toggledSelectionId = 0;
	int m_toggledVisibilityId = 0;
};

}

#endif
