#include "useritemdelegate.h"
#include "canvas/userlist.h"
#include "canvas/canvasmodel.h"
#include "canvas/aclfilter.h"
#include "utils/icon.h"
#include "net/commands.h"

#include <QPainter>
#include <QModelIndex>
#include <QMouseEvent>
#include <QMenu>

namespace widgets {

static const int MARGIN = 4;
static const int PADDING = 8;
static const int AVATAR_SIZE = 42;
static const int STATUS_OVERLAY_SIZE = 16;
static const int BUTTON_WIDTH = 16;

UserItemDelegate::UserItemDelegate(QObject *parent)
	: QAbstractItemDelegate(parent), m_canvas(nullptr)
{
	m_userMenu = new QMenu;
	m_opAction = m_userMenu->addAction(tr("Operator"));
	m_trustAction = m_userMenu->addAction(tr("Trusted"));

	m_userMenu->addSeparator();
	m_lockAction = m_userMenu->addAction(tr("Lock"));
	m_muteAction = m_userMenu->addAction(tr("Mute"));

	m_userMenu->addSeparator();
	m_kickAction = m_userMenu->addAction(tr("Kick"));
	m_banAction = m_userMenu->addAction(tr("Kick && Ban"));

	m_opAction->setCheckable(true);
	m_trustAction->setCheckable(true);
	m_lockAction->setCheckable(true);
	m_muteAction->setCheckable(true);

	connect(m_opAction, &QAction::triggered, this, &UserItemDelegate::toggleOpMode);
	connect(m_trustAction, &QAction::triggered, this, &UserItemDelegate::toggleTrusted);
	connect(m_lockAction, &QAction::triggered, this, &UserItemDelegate::toggleLock);
	connect(m_muteAction, &QAction::triggered, this, &UserItemDelegate::toggleMute);
	connect(m_kickAction, &QAction::triggered, this, &UserItemDelegate::kickUser);
	connect(m_banAction, &QAction::triggered, this, &UserItemDelegate::banUser);

	m_lockIcon = icon::fromTheme("object-locked").pixmap(16, 16);
	m_muteIcon = icon::fromTheme("irc-unvoice").pixmap(16, 16);
}

UserItemDelegate::~UserItemDelegate()
{
	delete m_userMenu;
}

QSize UserItemDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const
{
	Q_UNUSED(option);
	Q_UNUSED(index);

	return QSize(AVATAR_SIZE*4, AVATAR_SIZE+2*MARGIN);
}

void UserItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	painter->save();
	painter->fillRect(option.rect, option.backgroundBrush);
	painter->setRenderHint(QPainter::Antialiasing);

	// Draw avatar (placeholder)
	const QRect avatarRect(
		option.rect.x() + MARGIN,
		option.rect.y() + MARGIN,
		AVATAR_SIZE,
		AVATAR_SIZE
		);
	painter->setBrush(QColor("#eeefefff"));
	painter->setPen(Qt::NoPen);
	painter->drawEllipse(avatarRect);

	// Draw status overlay	
	const bool isLocked = index.data(canvas::UserListModel::IsLockedRole).toBool();
	if(isLocked || index.data(canvas::UserListModel::IsMutedRole).toBool()) {
		const QRect statusOverlayRect(
			avatarRect.right() - STATUS_OVERLAY_SIZE,
			avatarRect.bottom() - STATUS_OVERLAY_SIZE,
			STATUS_OVERLAY_SIZE,
			STATUS_OVERLAY_SIZE
			);

		painter->setBrush(option.palette.color(QPalette::AlternateBase));
		painter->drawEllipse(statusOverlayRect.adjusted(-2, -2, 2, 2));
		if(isLocked)
			painter->drawPixmap(statusOverlayRect, m_lockIcon);
		else
			painter->drawPixmap(statusOverlayRect, m_muteIcon);
	}

	// Draw username
	const QRect usernameRect(
		avatarRect.right() + PADDING,
		avatarRect.y(),
		option.rect.width() - avatarRect.right() - PADDING - BUTTON_WIDTH - MARGIN,
		option.rect.height() - MARGIN*2
		);
	QFont font = option.font;
	font.setPixelSize(22);
	font.setWeight(QFont::Light);
	painter->setPen(option.palette.text().color());
	painter->setFont(font);
	painter->drawText(usernameRect, index.data(canvas::UserListModel::NameRole).toString());

	// Draw user flags
	QString flags;

	if(index.data(canvas::UserListModel::IsModRole).toBool()) {
		flags = tr("Moderator");

	} else {
		if(index.data(canvas::UserListModel::IsOpRole).toBool())
			flags = tr("Operator");
		else if(index.data(canvas::UserListModel::IsTrustedRole).toBool())
			flags = tr("Trusted");

		if(index.data(canvas::UserListModel::IsAuthRole).toBool()) {
			if(!flags.isEmpty())
				flags += " | ";
			flags += tr("Registered");
		}
	}

	if(!flags.isEmpty()) {
		font.setPixelSize(12);
		font.setWeight(QFont::Normal);
		painter->setFont(font);
		painter->drawText(usernameRect, Qt::AlignBottom, flags);
	}

	// Draw the context menu buttons
	// The context menu contains commands for operators only
	if(m_canvas && m_canvas->aclFilter()->isLocalUserOperator()) {
		const QRect buttonRect(
			option.rect.right() - BUTTON_WIDTH - MARGIN,
			option.rect.top() + MARGIN,
			BUTTON_WIDTH,
			option.rect.height() - 2*MARGIN
			);

		painter->setPen(Qt::NoPen);
		//if((option.state & QStyle::State_MouseOver))
			painter->setBrush(option.palette.color(QPalette::WindowText));
		//else
			//painter->setBrush(option.palette.color(QPalette::AlternateBase));

		const int buttonSize = buttonRect.height()/7;
		for(int i=0;i<3;++i) {
			painter->drawEllipse(QRect(
				buttonRect.x() + (buttonRect.width()-buttonSize)/2,
				buttonRect.y() + (1+i*2) * buttonSize,
				buttonSize,
				buttonSize
			));
		}
	}

	painter->restore();
}

bool UserItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
	Q_UNUSED(model);

	if(event->type() == QEvent::MouseButtonPress && m_canvas && m_canvas->aclFilter()->isLocalUserOperator()) {
		const QMouseEvent *e = static_cast<const QMouseEvent*>(event);

		if(e->button() == Qt::RightButton || (e->button() == Qt::LeftButton && e->x() > option.rect.right() - MARGIN - BUTTON_WIDTH)) {
			showContextMenu(index, e->globalPos());
			return true;
		}
	}
	return false;
}

void UserItemDelegate::showContextMenu(const QModelIndex &index, const QPoint &pos)
{
	m_menuId = index.data(canvas::UserListModel::IdRole).toInt();

	m_opAction->setChecked(index.data(canvas::UserListModel::IsOpRole).toBool());
	m_trustAction->setChecked(index.data(canvas::UserListModel::IsTrustedRole).toBool());
	m_lockAction->setChecked(index.data(canvas::UserListModel::IsLockedRole).toBool());
	m_muteAction->setChecked(index.data(canvas::UserListModel::IsMutedRole).toBool());

	// Can't deop or kick self or moderators
	const bool enabled = m_menuId != m_canvas->localUserId() && !index.data(canvas::UserListModel::IsModRole).toBool();
	m_opAction->setEnabled(enabled);
	m_kickAction->setEnabled(enabled);
	m_banAction->setEnabled(enabled);

	m_userMenu->popup(pos);
}

void UserItemDelegate::toggleOpMode(bool op)
{
	emit opCommand(m_canvas->userlist()->getOpUserCommand(m_canvas->localUserId(), m_menuId, op));
}

void UserItemDelegate::toggleTrusted(bool trust)
{
	emit opCommand(m_canvas->userlist()->getTrustUserCommand(m_canvas->localUserId(), m_menuId, trust));
}

void UserItemDelegate::toggleLock(bool op)
{
	emit opCommand(m_canvas->userlist()->getLockUserCommand(m_canvas->localUserId(), m_menuId, op));
}

void UserItemDelegate::toggleMute(bool mute)
{
	emit opCommand(net::command::mute(m_menuId, mute));
}

void UserItemDelegate::kickUser()
{
	emit opCommand(net::command::kick(m_menuId, false));
}

void UserItemDelegate::banUser()
{
	emit opCommand(net::command::kick(m_menuId, true));
}

}
