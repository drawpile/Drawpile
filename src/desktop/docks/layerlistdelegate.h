// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DOCKS_LAYERLISTDELEGATE_H
#define DESKTOP_DOCKS_LAYERLISTDELEGATE_H
#include <QAbstractListModel>
#include <QIcon>
#include <QItemDelegate>

namespace docks {

/**
 * \brief A custom item delegate for displaying layer names and editing layer
 * settings.
 */
class LayerListDelegate final : public QItemDelegate {
	Q_OBJECT
public:
	LayerListDelegate(QObject *parent = nullptr);

	void paint(
		QPainter *painter, const QStyleOptionViewItem &option,
		const QModelIndex &index) const override;

	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index)
		const override;

	bool editorEvent(
		QEvent *event, QAbstractItemModel *model,
		const QStyleOptionViewItem &option, const QModelIndex &index) override;

	void setShowNumbers(bool show);

signals:
	void interacted();
	void toggleVisibility(int layerId, bool visible);
	void editProperties(QModelIndex index);

private:
	static constexpr int GLYPH_SIZE = 24;
	static constexpr int ICON_SIZE = 16;

	void drawOpacityGlyph(
		const QRectF &rect, QPainter *painter, float value, bool hidden,
		bool censored, bool group) const;

	void drawFillGlyph(const QRectF &rect, QPainter *painter) const;

	QIcon m_visibleIcon;
	QIcon m_groupIcon;
	QIcon m_censoredIcon;
	QIcon m_hiddenIcon;
	QIcon m_groupHiddenIcon;
	QIcon m_fillIcon;
	bool m_justToggledVisibility = false;
};

}

#endif
