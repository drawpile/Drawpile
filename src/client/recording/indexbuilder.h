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
#ifndef INDEXBUILDER_H
#define INDEXBUILDER_H

#include <QThread>
#include <QString>
#include <QHash>
#include <QAtomicInt>

#include "recording/filter.h"
#include "recording/index.h"

class ZipWriter;

namespace recording {

class Reader;

class IndexBuilder : public QThread
{
	Q_OBJECT
public:
	IndexBuilder(const QString &inputfile, const QString &targetfile, QObject *parent=0);

	void abort();

signals:
	void progress(int pos);
	void done(bool ok, const QString &msg);

protected:
	void run();

private:
	void addToIndex(const protocol::MessagePtr msg);
	void writeSnapshots(Reader &reader, ZipWriter &zip);

	QString _inputfile, _targetfile;
	QAtomicInt _abortflag;

	Index _index;
	qint64 _offset;
	int _pos;
	QHash<int, quint32> _colors;
};

}

#endif // INDEXBUILDER_H
