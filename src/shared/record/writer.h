/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#ifndef WRITER_H
#define WRITER_H

#include <QObject>
#include <QFile>
#include "../net/message.h"

namespace recording {

class Writer : public QObject
{
	Q_OBJECT
public:
	Writer(const QString &filename, QObject *parent=0);
	~Writer();

	QString errorString() const;

	bool open();
	void close();

	void setWriteIntervals(bool wi);

	bool writeHeader();

public slots:
	void recordMessage(const protocol::MessagePtr msg);

private:
	QFile _file;
	bool _writeIntervals;
	qint64 _interval;
};

}

#endif
