// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/chat/useritemdelegate.h"
#include "desktop/utils/qtguicompat.h"
#include "libclient/canvas/acl.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/userlist.h"
#include "libclient/document.h"
#include "libclient/net/message.h"
#include "libshared/net/servercmd.h"
#include <QIcon>
#include <QMenu>
#include <QModelIndex>
#include <QMouseEvent>
#include <QPainter>

namespace widgets {

static const int MARGIN = 4;
static const int PADDING = 8;
static const int AVATAR_SIZE = 32;
static const int STATUS_OVERLAY_SIZE = 16;
static const int BUTTON_WIDTH = 16;

UserItemDelegate::UserItemDelegate(QObject *parent)
	: QAbstractItemDelegate(parent)
	, m_doc(nullptr)
{
	m_userMenu = new QMenu;
	m_menuTitle = m_userMenu->addSection("User");

	m_opAction = m_userMenu->addAction(tr("Operator"));
	m_trustAction = m_userMenu->addAction(tr("Trusted"));

	m_userMenu->addSeparator();
	m_lockAction = m_userMenu->addAction(tr("Lock"));
	m_muteAction = m_userMenu->addAction(tr("Mute"));

	m_userMenu->addSeparator();
	m_undoAction = m_userMenu->addAction(tr("Undo"));
	m_redoAction = m_userMenu->addAction(tr("Redo"));

	m_userMenu->addSeparator();
	m_kickAction = m_userMenu->addAction(tr("Kick"));
	m_banAction = m_userMenu->addAction(tr("Kick && Ban"));

	m_userMenu->addSeparator();
	m_chatAction = m_userMenu->addAction(tr("Private Message"));
	m_infoAction = m_userMenu->addAction(tr("Show User Information"));
	m_brushAction = m_userMenu->addAction(tr("Take Current Brush"));

	m_opAction->setCheckable(true);
	m_trustAction->setCheckable(true);
	m_lockAction->setCheckable(true);
	m_muteAction->setCheckable(true);

	connect(
		m_opAction, &QAction::triggered, this, &UserItemDelegate::toggleOpMode);
	connect(
		m_trustAction, &QAction::triggered, this,
		&UserItemDelegate::toggleTrusted);
	connect(
		m_lockAction, &QAction::triggered, this, &UserItemDelegate::toggleLock);
	connect(
		m_muteAction, &QAction::triggered, this, &UserItemDelegate::toggleMute);
	connect(
		m_kickAction, &QAction::triggered, this, &UserItemDelegate::kickUser);
	connect(m_banAction, &QAction::triggered, this, &UserItemDelegate::banUser);
	connect(m_chatAction, &QAction::triggered, this, &UserItemDelegate::pmUser);
	connect(
		m_infoAction, &QAction::triggered, this,
		&UserItemDelegate::showUserInfo);
	connect(
		m_brushAction, &QAction::triggered, this,
		&UserItemDelegate::takeCurrentBrush);
	connect(
		m_undoAction, &QAction::triggered, this, &UserItemDelegate::undoByUser);
	connect(
		m_redoAction, &QAction::triggered, this, &UserItemDelegate::redoByUser);

	m_lockIcon = QIcon::fromTheme("object-locked");
	m_muteIcon = QIcon::fromTheme("irc-unvoice");
}

UserItemDelegate::~UserItemDelegate()
{
	delete m_userMenu;
}

void UserItemDelegate::setCompatibilityMode(bool compatibilityMode)
{
	m_infoAction->setDisabled(compatibilityMode);
}

QSize UserItemDelegate::sizeHint(
	const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	Q_UNUSED(option);
	Q_UNUSED(index);

	return QSize(AVATAR_SIZE * 4, AVATAR_SIZE + 2 * MARGIN);
}

void UserItemDelegate::paint(
	QPainter *painter, const QStyleOptionViewItem &option,
	const QModelIndex &index) const
{
	painter->save();
	painter->fillRect(option.rect, option.backgroundBrush);
	painter->setRenderHint(QPainter::Antialiasing);

	// Draw avatar
	const QRect avatarRect(
		option.rect.x() + MARGIN, option.rect.y() + MARGIN, AVATAR_SIZE,
		AVATAR_SIZE);
	painter->drawPixmap(
		avatarRect,
		index.data(canvas::UserListModel::AvatarRole).value<QPixmap>());

	// Draw status overlay
	const bool isLocked =
		index.data(canvas::UserListModel::IsLockedRole).toBool();
	if(isLocked || index.data(canvas::UserListModel::IsMutedRole).toBool()) {
		const QRect statusOverlayRect(
			avatarRect.right() - STATUS_OVERLAY_SIZE,
			avatarRect.bottom() - STATUS_OVERLAY_SIZE, STATUS_OVERLAY_SIZE,
			STATUS_OVERLAY_SIZE);

		painter->setBrush(option.palette.color(QPalette::AlternateBase));
		painter->setPen(QPen(option.palette.color(QPalette::Base), 2));
		painter->drawEllipse(statusOverlayRect.adjusted(-2, -2, 2, 2));
		if(isLocked)
			m_lockIcon.paint(painter, statusOverlayRect);
		else
			m_muteIcon.paint(painter, statusOverlayRect);
	}

	// Draw username
	const QRect usernameRect(
		avatarRect.right() + PADDING, avatarRect.y(),
		option.rect.width() - avatarRect.right() - PADDING - BUTTON_WIDTH -
			MARGIN,
		option.rect.height() - MARGIN * 2);
	QFont font = option.font;
	font.setPixelSize(16);
	font.setWeight(QFont::Light);
	painter->setPen(option.palette.text().color());
	painter->setFont(font);
	painter->drawText(
		usernameRect, Qt::AlignLeft | Qt::AlignTop,
		index.data(canvas::UserListModel::NameRole).toString());

	// Draw user flags
	QString flags;

	if(index.data(canvas::UserListModel::IsModRole).toBool()) {
		flags = tr("Moderator");

	} else {
		if(index.data(canvas::UserListModel::IsOpRole).toBool())
			flags = tr("Operator");
		else if(index.data(canvas::UserListModel::IsTrustedRole).toBool())
			flags = tr("Trusted");

		if(index.data(canvas::UserListModel::IsBotRole).toBool()) {
			if(!flags.isEmpty())
				flags += " | ";
			flags += tr("Bot");

		} else if(index.data(canvas::UserListModel::IsAuthRole).toBool()) {
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
	const QRect buttonRect(
		option.rect.right() - BUTTON_WIDTH - MARGIN, option.rect.top() + MARGIN,
		BUTTON_WIDTH, option.rect.height() - 2 * MARGIN);

	painter->setPen(Qt::NoPen);
	// if((option.state & QStyle::State_MouseOver))
	painter->setBrush(option.palette.color(QPalette::WindowText));
	// else
	// painter->setBrush(option.palette.color(QPalette::AlternateBase));

	const int buttonSize = buttonRect.height() / 7;
	for(int i = 0; i < 3; ++i) {
		painter->drawEllipse(QRect(
			buttonRect.x() + (buttonRect.width() - buttonSize) / 2,
			buttonRect.y() + (1 + i * 2) * buttonSize, buttonSize, buttonSize));
	}

	painter->restore();
}

bool UserItemDelegate::editorEvent(
	QEvent *event, QAbstractItemModel *model,
	const QStyleOptionViewItem &option, const QModelIndex &index)
{
	Q_UNUSED(model);

	if(event->type() == QEvent::MouseButtonPress && m_doc) {
		const QMouseEvent *e = static_cast<const QMouseEvent *>(event);

		if(e->button() == Qt::RightButton ||
		   (e->button() == Qt::LeftButton &&
			compat::mousePos(*e).x() >
				option.rect.right() - MARGIN - BUTTON_WIDTH)) {
			showContextMenu(index, compat::globalPos(*e));
			return true;
		}
	} else if(event->type() == QEvent::MouseButtonDblClick && m_doc) {
		const int userId = index.data(canvas::UserListModel::IdRole).toInt();
		if(userId > 0 && userId != m_doc->canvas()->localUserId()) {
			emit requestPrivateChat(userId);
			return true;
		}
	}
	return false;
}

void UserItemDelegate::showContextMenu(
	const QModelIndex &index, const QPoint &pos)
{
	m_menuId = index.data(canvas::UserListModel::IdRole).toInt();

	m_menuTitle->setText(
		index.data(canvas::UserListModel::NameRole).toString());

	bool amOp = m_doc->canvas()->aclState()->amOperator();
	bool amDeputy =
		m_doc->canvas()->aclState()->amTrusted() && m_doc->isSessionDeputies();
	bool isSelf = m_menuId == m_doc->canvas()->localUserId();
	bool isMod = index.data(canvas::UserListModel::IsModRole).toBool();
	bool isLocked = index.data(canvas::UserListModel::IsLockedRole).toBool();
	bool isMuted = index.data(canvas::UserListModel::IsMutedRole).toBool();

	m_opAction->setChecked(
		index.data(canvas::UserListModel::IsOpRole).toBool());
	m_trustAction->setChecked(
		index.data(canvas::UserListModel::IsTrustedRole).toBool());
	m_lockAction->setChecked(isLocked);
	m_muteAction->setChecked(isMuted);

	// Can't deop self or moderators
	m_opAction->setEnabled(amOp && !isSelf && !isMod);
	m_trustAction->setEnabled(amOp);
	// You're not supposed to be able to lock or mute moderators, but that's not
	// necessarily enforced server-side, so unlocking and unmuting is allowed.
	m_lockAction->setEnabled(amOp && (!isMod || isLocked));
	m_muteAction->setEnabled(amOp && (!isMod || isMuted));
	m_undoAction->setEnabled(amOp);
	m_redoAction->setEnabled(amOp);

	// Deputies can only kick non-trusted users
	// No-one can kick themselves or moderators
	bool canKick =
		!isSelf && !isMod &&
		(amOp ||
		 (amDeputy &&
		  !(index.data(canvas::UserListModel::IsOpRole).toBool() ||
			index.data(canvas::UserListModel::IsTrustedRole).toBool())));
	m_kickAction->setEnabled(canKick);
	m_banAction->setEnabled(canKick);

	m_chatAction->setEnabled(!isSelf);	// Can't chat with self.
	m_brushAction->setEnabled(!isSelf); // Taking your own brush is pointless.

	m_userMenu->popup(pos);
}

void UserItemDelegate::toggleOpMode(bool op)
{
	emit opCommand(m_doc->canvas()->userlist()->getOpUserCommand(
		m_doc->canvas()->localUserId(), m_menuId, op));
}

void UserItemDelegate::toggleTrusted(bool trust)
{
	emit opCommand(m_doc->canvas()->userlist()->getTrustUserCommand(
		m_doc->canvas()->localUserId(), m_menuId, trust));
}

void UserItemDelegate::toggleLock(bool op)
{
	emit opCommand(m_doc->canvas()->userlist()->getLockUserCommand(
		m_doc->canvas()->localUserId(), m_menuId, op));
}

void UserItemDelegate::toggleMute(bool mute)
{
	emit opCommand(net::ServerCommand::makeMute(m_menuId, mute));
}

void UserItemDelegate::kickUser()
{
	emit opCommand(net::ServerCommand::makeKick(m_menuId, false));
}

void UserItemDelegate::banUser()
{
	emit opCommand(net::ServerCommand::makeKick(m_menuId, true));
}

void UserItemDelegate::pmUser()
{
	emit requestPrivateChat(m_menuId);
}

void UserItemDelegate::showUserInfo()
{
	emit requestUserInfo(m_menuId);
}

void UserItemDelegate::takeCurrentBrush()
{
	emit requestCurrentBrush(m_menuId);
}

void UserItemDelegate::undoByUser()
{
	emit opCommand(
		net::makeUndoMessage(m_doc->canvas()->localUserId(), m_menuId, false));
}

void UserItemDelegate::redoByUser()
{
	emit opCommand(
		net::makeUndoMessage(m_doc->canvas()->localUserId(), m_menuId, true));
}

}
