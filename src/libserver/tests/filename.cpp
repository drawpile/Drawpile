// SPDX-License-Identifier: GPL-3.0-or-later

#include "libserver/util/filename.h"
#include <dpcommon/platform_qt.h>
#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>

class TestFilename final : public QObject
{
	Q_OBJECT
private slots:
	void testUniqueFilenameGeneration()
	{
		QTemporaryDir tempdir;
		QVERIFY(tempdir.isValid());
		QDir dir(tempdir.path());

		QVERIFY(touch(dir.filePath("hello.txt")));

		// Non-existenting file: no change to filename
		QFileInfo uniq1 = utils::uniqueFilename(dir, "test", "txt");
		QCOMPARE(uniq1.fileName(), QString("test.txt"));

		// Existing file: file name must be changed
		QStringList uniqueNames;
		uniqueNames << "hello.txt";

		for(int tries=0;tries<10;++tries) {
			QFileInfo uniq2 = utils::uniqueFilename(dir, "hello", "txt");
			QVERIFY(!uniqueNames.contains(uniq2.fileName()));
			QVERIFY(touch(uniq2.filePath()));
			uniqueNames << uniq2.fileName();
		}
	}

private:
	bool touch(const QString &path)
	{
		QFile f { path };
		if(!f.open(DP_QT_WRITE_FLAGS))
			return false;
		f.close();
		return true;
	}
};


QTEST_MAIN(TestFilename)
#include "filename.moc"

