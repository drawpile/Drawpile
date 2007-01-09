
/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#include <QVBoxLayout>
#include <QPainter>
#include <QLabel>
#include <QBitmap>

#include "popupmessage.h"

namespace widgets {

PopupMessage::PopupMessage(QWidget *parent)
	: QWidget(parent, Qt::Popup)
{
	QVBoxLayout *layout = new QVBoxLayout(this);
	message_ = new QLabel(this);
	message_->setAlignment(Qt::AlignCenter);
	message_->setTextFormat(Qt::RichText);
	layout->addWidget(message_);
	resize(200,60);
	timer_.setSingleShot(true);
	timer_.setInterval(2500);
	connect(&timer_, SIGNAL(timeout()), this, SLOT(hide()));
}

/**
 * @param message message to display
 */
void PopupMessage::setMessage(const QString& message)
{
	if(isVisible())
		message_->setText(message_->text() + "<br>" + message);
	else
		message_->setText(message);
}

/**
 * The message will popup with the lower left corner align at the
 * specified point.
 * @param point popup coordinates
 */
void PopupMessage::popupAt(const QPoint& point)
{
	move(point - QPoint(0,height()));
	show();
	timer_.start();
}

void PopupMessage::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	QRect rect = contentsRect().adjusted(0,0,-1,-1);
	int xrnd = qRound(20 * height()/double(width()));
	painter.setBrush(palette().light());
	painter.drawRoundRect(rect,xrnd,20);
}

void PopupMessage::resizeEvent(QResizeEvent *)
{
	QBitmap mask(width(), height());
	mask.fill(Qt::color0);
	QPainter painter(&mask);
	painter.setBrush(Qt::color1);
	QRect rect = contentsRect().adjusted(0,0,-1,-1);
	int xrnd = qRound(20 * height()/double(width()));
	painter.drawRoundRect(rect,xrnd,20);
	setMask(mask);
}

}

