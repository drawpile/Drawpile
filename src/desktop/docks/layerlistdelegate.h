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

signals:
	void interacted();
	void toggleVisibility(int layerId, bool visible);
	void toggleChecked(int layerId, bool checked);
	void editProperties(QModelIndex index);

private:
	static constexpr int GLYPH_SIZE = 24;
	static constexpr int ICON_SIZE = 16;

	static QRect getOpacityGlyphRect(const QStyleOptionViewItem &opt);
	static QRect getCheckRect(const QStyleOptionViewItem &opt);
	static bool hasCheckBox(int checkState);

	void drawOpacityGlyph(
		const QRect &rect, QPainter *painter, float value, bool hidden,
		bool censored, bool group) const;

	void drawSelectionCheckBox(
		const QRect &rect, QPainter *painter,
		const QStyleOptionViewItem &option, int checkState) const;

	void drawFillGlyph(const QRect &rect, QPainter *painter) const;

	QIcon m_visibleIcon;
	QIcon m_groupIcon;
	QIcon m_censoredIcon;
	QIcon m_hiddenIcon;
	QIcon m_groupHiddenIcon;
	QIcon m_fillIcon;
	QIcon m_forbiddenIcon;
	bool m_justToggledVisibility = false;
};

}

#endif
