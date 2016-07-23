/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2016 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef INDEXBUILDER_H
#define INDEXBUILDER_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QAtomicInt>

#include "recording/filter.h"
#include "recording/index.h"

class KZip;

namespace recording {

class Reader;

class IndexBuilder : public QObject
{
	Q_OBJECT
public:
	IndexBuilder(const QString &inputfile, const QString &targetfile, QObject *parent=0);

	//! Abort index building (thread-safe)
	void abort();

public slots:
	void run();

signals:
	void progress(int pos);
	void done(bool ok, const QString &msg);

private:
	void generateIndex(KZip &zip, Reader &reader);

	QString m_inputfile, m_targetfile;
	QAtomicInt m_abortflag;

	Index m_index;
};

}

#endif // INDEXBUILDER_H

