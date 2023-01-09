#include "../util/filename.h"

#include <QtTest/QtTest>
#include <QTemporaryDir>
#include <QDir>

class TestFilename: public QObject
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
		QFileInfo uniq1 = QFileInfo(utils::uniqueFilename(dir, "test", "txt"));
		QCOMPARE(uniq1.fileName(), QString("test.txt"));

		// Existing file: file name must be changed
		QStringList uniqueNames;
		uniqueNames << "hello.txt";

		for(int tries=0;tries<10;++tries) {
			QFileInfo uniq2 = QFileInfo(utils::uniqueFilename(dir, "hello", "txt"));
			QVERIFY(!uniqueNames.contains(uniq2.fileName()));
			QVERIFY(touch(uniq2.filePath()));
			uniqueNames << uniq2.fileName();
		}
	}

private:
	bool touch(const QString &path)
	{
		QFile f { path };
		if(!f.open(QIODevice::WriteOnly))
			return false;
		f.close();
		return true;
	}
};


QTEST_MAIN(TestFilename)
#include "filename.moc"

