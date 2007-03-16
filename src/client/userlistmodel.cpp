#include <QDebug>
#include <QStyleOptionViewItemV2>
#include <QPainter>
#include <QMouseEvent>
#include "userlistmodel.h"
#include "netstate.h"

Q_DECLARE_METATYPE(network::User)

QVariant UserListModel::data(const QModelIndex& index, int role) const
{
	if(index.row() < 0 || index.row() >= users_.size())
		return QVariant();
	if(role == Qt::DisplayRole)
		return QVariant::fromValue(users_.at(index.row()));
	return QVariant();
}

int UserListModel::rowCount(const QModelIndex& parent) const
{
	if(parent.isValid())
		return 0;
	return users_.count();
}

void UserListModel::addUser(const network::User& user)
{
	beginInsertRows(QModelIndex(),users_.size(),users_.size());
	users_.append(user);
	endInsertRows();
}

void UserListModel::changeUser(const network::User& user)
{
	for(int i=0;i<users_.size();++i) {
		if(users_.at(i).id() == user.id()) {
			users_[i] = user;
			QModelIndex ind = index(i,0);
			emit dataChanged(ind,ind);
			break;
		}
	}
}

bool UserListModel::hasUser(int id) const
{
	for(int i=0;i<users_.size();++i)
		if(users_.at(i).id() == id)
			return true;
	return false;
}

void UserListModel::removeUser(int id)
{
	for(int i=0;i<users_.size();++i) {
		if(users_.at(i).id() == id) {
			beginRemoveRows(QModelIndex(),i,i);
			users_.removeAt(i);
			endRemoveRows();
			break;
		}
	}
}

void UserListModel::clearUsers()
{
	if(users_.isEmpty()==false) {
		beginRemoveRows(QModelIndex(), 0, users_.size()-1);
		users_.clear();
		endRemoveRows();
	}
}

UserListDelegate::UserListDelegate(QObject *parent)
	: QItemDelegate(parent), enableadmin_(false), lock_(":icons/lock.png"),
	unlock_(":icons/unlock.png"), kick_(":icons/kick.png")
{
}

/**
 * If admin mode is enabled, lock and kick buttons are visible and active.
 * Note that enabling admin mode should not be enabled if the user is not
 * actually the session owner, as the server will automatically kick users
 * who try to use admin commands without permission.
 * @param enable if true, admin command buttons are visible
 */
void UserListDelegate::setAdminMode(bool enable)
{
	enableadmin_ = enable;
}

void UserListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QStyleOptionViewItemV2 opt = setOptions(index, option);
	const QStyleOptionViewItemV2 *v2 = qstyleoption_cast<const QStyleOptionViewItemV2 *>(&option);
	opt.features = v2 ? v2->features : QStyleOptionViewItemV2::ViewItemFeatures(QStyleOptionViewItemV2::None);

	network::User user = index.data().value<network::User>();
	//qDebug() << user.name;

	// Background
	drawBackground(painter, opt, index);

	// Lock button/indicator. This is shown even when not in admin mode.
	painter->drawPixmap(opt.rect.topLeft(), user.locked()?lock_:unlock_);
	QRect textrect = opt.rect;

	// Name
	textrect.setX(kick_.width() + 5);
	drawDisplay(painter, opt, textrect, user.name());

	// Kick button (only in admin mode)
	if(enableadmin_)
		painter->drawPixmap(opt.rect.topRight()-QPoint(kick_.width(),0), kick_);
}

QSize UserListDelegate::sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
	QSize size = QItemDelegate::sizeHint(option, index);
	if(size.height() < lock_.height())
		size.setHeight(lock_.height());
	return size;
}

bool UserListDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
	if(enableadmin_ && event->type() == QEvent::MouseButtonPress) {
		QMouseEvent *me = static_cast<QMouseEvent*>(event);

		network::User user = index.data().value<network::User>();
		if(me->x() <= lock_.width()) {
			// User pressed lock button
			emit lockUser(user.id(), !user.locked());
			return true;
		} else if(me->x() >= option.rect.width()-kick_.width()) {
			// User pressed kick button
			emit kickUser(user.id());
			return true;
		}
	}
	return QItemDelegate::editorEvent(event, model, option, index);
}

