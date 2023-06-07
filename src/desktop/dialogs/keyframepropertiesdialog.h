// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef KEYFRAMEPROPERTIESDIALOG_H
#define KEYFRAMEPROPERTIESDIALOG_H

#include <QDialog>
#include <QItemDelegate>

class KeyFrameLayerModel;
class QAbstractButton;
class QDialogButtonBox;
class QLineEdit;
class QTreeView;
struct KeyFrameLayerItem;

namespace dialogs {

class KeyFramePropertiesDialogLayerDelegate final : public QItemDelegate {
	Q_OBJECT
public:
	static constexpr QSize ICON_SIZE{16, 16};

	KeyFramePropertiesDialogLayerDelegate(QObject *parent = nullptr);

	void paint(
		QPainter *painter, const QStyleOptionViewItem &option,
		const QModelIndex &index) const override;

	bool editorEvent(
		QEvent *event, QAbstractItemModel *model,
		const QStyleOptionViewItem &option, const QModelIndex &index) override;

	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index)
		const override;

signals:
	void toggleVisibility(int layerId, bool revealed);

private:
	void drawIcon(
		QPainter *painter, const QRectF &rect, const QIcon &icon,
		bool enabled) const;

	static QRect hiddenRect(const QRect &rect);
	static QRect revealedRect(const QRect &rect);
	static QRect textRect(const QRect &rect);

	QIcon m_hiddenIcon;
	QIcon m_revealedIcon;
};


class KeyFramePropertiesDialog final : public QDialog {
	Q_OBJECT
public:
	explicit KeyFramePropertiesDialog(
		int trackId, int frame, QWidget *parent = nullptr);

	int trackId() const { return m_trackId; }
	int frame() const { return m_frame; }

	void setKeyFrameTitle(const QString &title);
	void setKeyFrameLayers(KeyFrameLayerModel *layerModel);

signals:
	void keyFramePropertiesChanged(
		int trackId, int frame, const QString &title,
		const QHash<int, bool> layerVisibility);

private slots:
	void buttonClicked(QAbstractButton *button);

private:
	int m_trackId;
	int m_frame;
	KeyFrameLayerModel *m_layerModel;
	KeyFramePropertiesDialogLayerDelegate *m_layerDelegate;
	QLineEdit *m_titleEdit;
	QTreeView *m_layerTree;
	QDialogButtonBox *m_buttons;
};

}

#endif
