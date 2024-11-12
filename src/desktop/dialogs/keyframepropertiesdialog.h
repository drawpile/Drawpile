// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_KEYFRAMEPROPERTIESDIALOG_H
#define DESKTOP_DIALOGS_KEYFRAMEPROPERTIESDIALOG_H
#include <QDialog>
#include <QItemDelegate>
#include <QModelIndexList>

class KeyFrameLayerModel;
class QAbstractButton;
class QDialogButtonBox;
class QLineEdit;
class QTreeView;
struct KeyFrameLayerItem;

namespace widgets {
class GroupedToolButton;
}

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

protected:
	void keyPressEvent(QKeyEvent *event) override;
	void showEvent(QShowEvent *event) override;

private:
	void updateSearch(const QString &text);
	void updateSearchRecursive(
		const QString &search, const QModelIndex &parent,
		const QModelIndex &current);
	void selectPreviousSearchResult();
	void selectNextSearchResult();
	void buttonClicked(QAbstractButton *button);

	int m_trackId;
	int m_frame;
	KeyFrameLayerModel *m_layerModel = nullptr;
	KeyFramePropertiesDialogLayerDelegate *m_layerDelegate;
	QLineEdit *m_titleEdit;
	QLineEdit *m_searchEdit;
	widgets::GroupedToolButton *m_searchPrevButton;
	widgets::GroupedToolButton *m_searchNextButton;
	QTreeView *m_layerTree;
	QDialogButtonBox *m_buttons;
	QModelIndexList m_searchResults;
	int m_searchResultIndex = -1;
};

}

#endif
