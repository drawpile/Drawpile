/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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

#include "recording/index.h"

#include <QObject>
#include <QString>
#include <QAtomicInt>

class QDataStream;

namespace recording {

class Reader;

class IndexBuilder : public QObject
{
	Q_OBJECT
public:
	IndexBuilder(const QString &inputfile, const QString &targetfile, const QByteArray &hash, QObject *parent=nullptr);

	//! Abort index building (thread-safe)
	void abort();

public slots:
	void run();

signals:
	void progress(int pos);
	void done(bool ok, const QString &msg);

private:
	bool generateIndex(QDataStream &stream, Reader &reader);

	QString m_inputfile, m_targetfile;
	QByteArray m_recordingHash;
	QAtomicInt m_abortflag;

	QVector<IndexEntry> m_index;
	int m_messageCount;
};

}

#endif // INDEXBUILDER_H

