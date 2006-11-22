#include <QContextMenuEvent>
#include <QApplication>
#include <QClipboard>
#include <QMenu>

#include "hostlabel.h"

HostLabel::HostLabel(QWidget *parent)
	:QLabel(tr("not connected"), parent)
{
	setTextInteractionFlags(Qt::TextSelectableByMouse);
	setCursor(Qt::IBeamCursor);
	menu_ = new QMenu(this);
	QAction *copy = menu_->addAction(tr("Copy address to clipboard"));
	connect(copy,SIGNAL(triggered()),this,SLOT(copyAddress()));
	copy->setEnabled(false);
	menu_->setDefaultAction(copy);
}

void HostLabel::setAddress(QString address)
{
	address_ = address;
	setText("Host: " + address_);
	menu_->defaultAction()->setEnabled(true);
}

void HostLabel::disconnect()
{
	address_ = QString();
	setText(tr("not connected"));
	menu_->defaultAction()->setEnabled(false);

}

void HostLabel::copyAddress()
{
	QApplication::clipboard()->setText(address_);
}

void HostLabel::contextMenuEvent(QContextMenuEvent *event)
{
	menu_->popup(event->globalPos());
}

