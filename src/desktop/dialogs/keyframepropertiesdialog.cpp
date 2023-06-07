// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/keyframepropertiesdialog.h"
#include "libclient/utils/keyframelayermodel.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTreeView>

namespace dialogs {


KeyFramePropertiesDialogLayerDelegate::KeyFramePropertiesDialogLayerDelegate(
	QObject *parent)
	: QItemDelegate{parent}
	, m_hiddenIcon{QIcon::fromTheme("view-hidden")}
	, m_revealedIcon{QIcon::fromTheme("view-visible")}
{
}

void KeyFramePropertiesDialogLayerDelegate::paint(
	QPainter *painter, const QStyleOptionViewItem &option,
	const QModelIndex &index) const
{
	QStyleOptionViewItem opt = setOptions(index, option);
	painter->save();
	drawBackground(painter, option, index);

	const KeyFrameLayerItem &item = index.data().value<KeyFrameLayerItem>();
	drawIcon(
		painter, hiddenRect(opt.rect), m_hiddenIcon,
		item.visibility == KeyFrameLayerItem::Visibility::Hidden);
	drawIcon(
		painter, revealedRect(opt.rect), m_revealedIcon,
		item.visibility == KeyFrameLayerItem::Visibility::Revealed);

	if(!index.data(KeyFrameLayerModel::IsVisibleRole).toBool()) {
		opt.state &= ~QStyle::State_Enabled;
	}
	drawDisplay(painter, opt, textRect(opt.rect), item.title);
	painter->restore();
}

bool KeyFramePropertiesDialogLayerDelegate::editorEvent(
	QEvent *event, QAbstractItemModel *model,
	const QStyleOptionViewItem &option, const QModelIndex &index)
{
	if(event->type() == QEvent::MouseButtonPress) {
		QMouseEvent *me = static_cast<QMouseEvent *>(event);
		if(me->button() == Qt::LeftButton) {
			QPoint pos = me->pos();
			if(hiddenRect(option.rect).contains(pos)) {
				emit toggleVisibility(
					index.data().value<KeyFrameLayerItem>().id, false);
				return true;
			} else if(revealedRect(option.rect).contains(pos)) {
				emit toggleVisibility(
					index.data().value<KeyFrameLayerItem>().id, true);
				return true;
			}
		}
	}
	return QItemDelegate::editorEvent(event, model, option, index);
}

QSize KeyFramePropertiesDialogLayerDelegate::sizeHint(
	const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QSize size = QItemDelegate::sizeHint(option, index);
	QFontMetrics fm(option.font);
	int minHeight = qMax(fm.height() * 3 / 2, ICON_SIZE.height()) + 2;
	if(size.height() < minHeight) {
		size.setHeight(minHeight);
	}
	return size;
}

void KeyFramePropertiesDialogLayerDelegate::drawIcon(
	QPainter *painter, const QRectF &rect, const QIcon &icon,
	bool enabled) const
{
	QRect r{
		int(rect.left() + rect.width() / 2.0 - ICON_SIZE.width() / 2.0),
		int(rect.top() + rect.height() / 2.0 - ICON_SIZE.height() / 2.0),
		ICON_SIZE.width(),
		ICON_SIZE.height(),
	};
	painter->save();
	painter->setOpacity(enabled ? 1.0 : 0.2);
	icon.paint(painter, r);
	painter->restore();
}

QRect KeyFramePropertiesDialogLayerDelegate::hiddenRect(const QRect &rect)
{
	return QRect{
		rect.topLeft() + QPoint(0, rect.height() / 2 - 12), QSize(24, 24)};
}

QRect KeyFramePropertiesDialogLayerDelegate::revealedRect(const QRect &rect)
{
	QRect r = hiddenRect(rect);
	r.moveLeft(r.right());
	return r;
}

QRect KeyFramePropertiesDialogLayerDelegate::textRect(const QRect &rect)
{
	QRect r = rect;
	r.setLeft(revealedRect(rect).right());
	return r;
}


KeyFramePropertiesDialog::KeyFramePropertiesDialog(
	int trackId, int frame, QWidget *parent)
	: QDialog{parent}
	, m_trackId{trackId}
	, m_frame{frame}
	, m_layerModel{nullptr}
	, m_layerDelegate{new KeyFramePropertiesDialogLayerDelegate{this}}
{
	setWindowTitle(tr("Key Frame Properties"));
	resize(400, 400);

	QFormLayout *layout = new QFormLayout{this};

	m_titleEdit = new QLineEdit{this};
	layout->addRow(tr("Title:"), m_titleEdit);

	m_layerTree = new QTreeView{this};
	m_layerTree->setEnabled(false);
	m_layerTree->setHeaderHidden(true);
	m_layerTree->setItemDelegate(m_layerDelegate);
	layout->addRow(tr("Filter Layers:"), m_layerTree);

	m_buttons = new QDialogButtonBox{
		QDialogButtonBox::Ok | QDialogButtonBox::Apply |
			QDialogButtonBox::Cancel,
		this};
	layout->addRow(m_buttons);

	connect(
		m_buttons, &QDialogButtonBox::clicked, this,
		&KeyFramePropertiesDialog::buttonClicked);
}

void KeyFramePropertiesDialog::setKeyFrameTitle(const QString &title)
{
	m_titleEdit->setText(title);
	m_titleEdit->selectAll();
}

void KeyFramePropertiesDialog::setKeyFrameLayers(KeyFrameLayerModel *layerModel)
{
	if(m_layerModel) {
		m_layerModel->deleteLater();
	}

	m_layerModel = layerModel;
	m_layerTree->setModel(layerModel);
	m_layerTree->setEnabled(layerModel != nullptr);
	if(layerModel) {
		layerModel->setParent(this);
		m_layerTree->expandAll();
		connect(
			m_layerDelegate,
			&KeyFramePropertiesDialogLayerDelegate::toggleVisibility,
			layerModel, &KeyFrameLayerModel::toggleVisibility);
	}
}

void KeyFramePropertiesDialog::buttonClicked(QAbstractButton *button)
{
	bool okPressed = m_buttons->button(QDialogButtonBox::Ok) == button;
	bool applyPressed = m_buttons->button(QDialogButtonBox::Apply) == button;
	bool cancelPressed = m_buttons->button(QDialogButtonBox::Cancel) == button;

	if(okPressed || applyPressed) {
		emit keyFramePropertiesChanged(
			m_trackId, m_frame, m_titleEdit->text(),
			m_layerModel ? m_layerModel->layerVisibility()
						 : QHash<int, bool>{});
	}

	if(okPressed) {
		accept();
	} else if(cancelPressed) {
		reject();
	}
}

}
