// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/keyframepropertiesdialog.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "libclient/utils/keyframelayermodel.h"
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTreeView>

namespace dialogs {


KeyFramePropertiesDialogLayerDelegate::KeyFramePropertiesDialogLayerDelegate(
	QObject *parent)
	: QItemDelegate(parent)
	, m_hiddenIcon(QIcon::fromTheme("view-hidden"))
	, m_revealedIcon(QIcon::fromTheme("view-visible"))
{
}

void KeyFramePropertiesDialogLayerDelegate::paint(
	QPainter *painter, const QStyleOptionViewItem &option,
	const QModelIndex &index) const
{
	QStyleOptionViewItem opt = setOptions(index, option);
	if(!index.data(KeyFrameLayerModel::IsVisibleRole).toBool()) {
		opt.state &= ~QStyle::State_Enabled;
	}

	drawBackground(painter, opt, index);

	const KeyFrameLayerItem &item = index.data().value<KeyFrameLayerItem>();
	drawIcon(
		painter, hiddenRect(opt.rect), m_hiddenIcon,
		item.visibility == KeyFrameLayerItem::Visibility::Hidden);
	drawIcon(
		painter, revealedRect(opt.rect), m_revealedIcon,
		item.visibility == KeyFrameLayerItem::Visibility::Revealed);

	drawDisplay(painter, opt, textRect(opt.rect), item.title);
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
	: QDialog(parent)
	, m_trackId(trackId)
	, m_frame(frame)
	, m_layerDelegate(new KeyFramePropertiesDialogLayerDelegate(this))
{
	setWindowTitle(tr("Key Frame Properties"));
	resize(400, 400);

	QFormLayout *layout = new QFormLayout(this);

	m_titleEdit = new QLineEdit;
	layout->addRow(tr("Title:"), m_titleEdit);

	QHBoxLayout *searchLayout = new QHBoxLayout;
	searchLayout->setSpacing(0);
	layout->addRow(searchLayout);

	m_searchEdit = new QLineEdit;
	m_searchEdit->setClearButtonEnabled(true);
	m_searchEdit->setPlaceholderText(tr("Search…"));
	m_searchEdit->addAction(
		QIcon::fromTheme("edit-find"), QLineEdit::LeadingPosition);
	searchLayout->addWidget(m_searchEdit, 1);
	connect(
		m_searchEdit, &QLineEdit::textChanged, this,
		&KeyFramePropertiesDialog::updateSearch);

	searchLayout->addSpacing(4);

	m_searchPrevButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupLeft);
	m_searchPrevButton->setIcon(QIcon::fromTheme("arrow-up"));
	m_searchPrevButton->setToolTip(tr("Previous"));
	m_searchPrevButton->setEnabled(false);
	searchLayout->addWidget(m_searchPrevButton);
	connect(
		m_searchPrevButton, &widgets::GroupedToolButton::clicked, this,
		&KeyFramePropertiesDialog::selectPreviousSearchResult);

	m_searchNextButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	m_searchNextButton->setIcon(QIcon::fromTheme("arrow-down"));
	m_searchNextButton->setToolTip(tr("Next"));
	m_searchNextButton->setEnabled(false);
	searchLayout->addWidget(m_searchNextButton);
	connect(
		m_searchNextButton, &widgets::GroupedToolButton::clicked, this,
		&KeyFramePropertiesDialog::selectNextSearchResult);

	m_layerTree = new QTreeView;
	m_layerTree->setEnabled(false);
	m_layerTree->setHeaderHidden(true);
	m_layerTree->setItemDelegate(m_layerDelegate);
	m_layerTree->setSelectionMode(QAbstractItemView::SingleSelection);

	utils::bindKineticScrolling(m_layerTree);
	layout->addRow(m_layerTree);

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

void KeyFramePropertiesDialog::keyPressEvent(QKeyEvent *event)
{
	if(m_searchEdit->hasFocus()) {
		switch(event->key()) {
		case Qt::Key_Return:
		case Qt::Key_Enter: {
			Qt::KeyboardModifiers mods = event->modifiers();
			if(mods.testFlag(Qt::ControlModifier)) {
				m_buttons->button(QDialogButtonBox::Ok)->click();
			} else if(mods.testFlag(Qt::ShiftModifier)) {
				selectPreviousSearchResult();
			} else {
				selectNextSearchResult();
			}
			return;
		}
		case Qt::Key_Down:
		case Qt::Key_PageDown:
			selectNextSearchResult();
			return;
		case Qt::Key_Up:
		case Qt::Key_PageUp:
			selectPreviousSearchResult();
			return;
		default:
			break;
		}
	}
	QDialog::keyPressEvent(event);
}

void KeyFramePropertiesDialog::showEvent(QShowEvent *event)
{
	QDialog::showEvent(event);
	m_searchEdit->setFocus();
}

void KeyFramePropertiesDialog::updateSearch(const QString &text)
{
	QString search = text.trimmed();
	m_searchResults.clear();
	m_searchResultIndex = 0;
	if(!search.isEmpty() && m_layerModel) {
		updateSearchRecursive(
			search, QModelIndex(), m_layerTree->currentIndex());
	}

	int resultCount = m_searchResults.size();
	if(m_searchResultIndex >= resultCount) {
		m_searchResultIndex = 0;
	}

	bool haveResults = resultCount != 0;
	m_searchNextButton->setEnabled(haveResults);
	m_searchPrevButton->setEnabled(haveResults);
	if(haveResults) {
		const QModelIndex idx = m_searchResults[m_searchResultIndex];
		m_layerTree->scrollTo(idx, QTreeView::PositionAtCenter);
		m_layerTree->setCurrentIndex(idx);
	}
}

void KeyFramePropertiesDialog::updateSearchRecursive(
	const QString &search, const QModelIndex &parent,
	const QModelIndex &current)
{
	int count = m_layerModel->rowCount(parent);
	for(int i = 0; i < count; ++i) {
		QModelIndex idx = m_layerModel->index(i, 0, parent);
		if(current.isValid() && idx == current) {
			m_searchResultIndex = m_searchResults.count();
		}

		const KeyFrameLayerItem &item = idx.data().value<KeyFrameLayerItem>();
		if(item.title.contains(search, Qt::CaseInsensitive)) {
			m_searchResults.append(idx);
		}

		updateSearchRecursive(search, idx, current);
	}
}

void KeyFramePropertiesDialog::selectPreviousSearchResult()
{
	int count = m_searchResults.size();
	if(count != 0) {
		m_searchResultIndex =
			m_searchResultIndex > 0 ? m_searchResultIndex - 1 : count - 1;
		m_layerTree->setCurrentIndex(m_searchResults[m_searchResultIndex]);
	}
}

void KeyFramePropertiesDialog::selectNextSearchResult()
{
	int count = m_searchResults.size();
	if(count != 0) {
		m_searchResultIndex =
			m_searchResultIndex < count - 1 ? m_searchResultIndex + 1 : 0;
		m_layerTree->setCurrentIndex(m_searchResults[m_searchResultIndex]);
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
						 : QHash<int, bool>());
	}

	if(okPressed) {
		accept();
	} else if(cancelPressed) {
		reject();
	}
}

}
