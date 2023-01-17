// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LAYERLISTMODEL_H
#define LAYERLISTMODEL_H

#include <QAbstractListModel>
#include <QIcon>
#include <QItemDelegate>

namespace docks {

/**
 * \brief A custom item delegate for displaying layer names and editing layer settings.
 */
class LayerListDelegate final : public QItemDelegate {
	Q_OBJECT
public:
	LayerListDelegate(QObject *parent=nullptr);

	void paint(QPainter *painter, const QStyleOptionViewItem &option,
			const QModelIndex &index) const override;
	QSize sizeHint(const QStyleOptionViewItem & option,
			const QModelIndex & index ) const override;

	void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem & option, const QModelIndex & index ) const override;
	bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;

	void setShowNumbers(bool show);

signals:
	void toggleVisibility(int layerId, bool visible);
	void editProperties(QModelIndex index);

private:
	void drawOpacityGlyph(const QRectF& rect, QPainter *painter, float value, bool hidden, bool censored, bool group) const;

	QIcon m_visibleIcon;
	QIcon m_groupIcon;
	QIcon m_censoredIcon;
	QIcon m_hiddenIcon;
};

}

#endif
