#include "../canvas/aclfilter.h"
#include "../../shared/net/textmode.h"

#include <QtTest/QtTest>

using namespace protocol;
using namespace canvas;

class TestAclFilter : public QObject
{
	Q_OBJECT
private slots:
	// Test the default filtering of commands
	void testFilterInitialStateLocal_data()
	{
		QTest::addColumn<bool>("allowed");

		// Layers can be created and edited by all users by default
		QTest::newRow("1 newlayer") << true;
		QTest::newRow("2 newlayer id=0x0101") << false; // Non-ops must obey ID prefixing rules
		QTest::newRow("2 newlayer id=0x0201") << true;
		QTest::newRow("1 layerattr") << true;
		QTest::newRow("2 layerattr") << false;
		QTest::newRow("1 layervisibility") << true;
		QTest::newRow("2 layervisibility") << true;
		QTest::newRow("1 retitlelayer") << true;
		QTest::newRow("2 retitlelayer") << false;
		QTest::newRow("1 layerorder") << true;
		QTest::newRow("2 layerorder") << false;
		QTest::newRow("1 deletelayer") << true;
		QTest::newRow("2 deletelayer") << false;

		// Regular undo is usable by all, but override needs Op
		QTest::newRow("1 undo") << true;
		QTest::newRow("2 undo") << true;
		QTest::newRow("1 undo override=2") << true;
		QTest::newRow("2 undo override=1") << false;

		// Op-only commands
		QTest::newRow("0 owner") << true; // server can send this one too
		QTest::newRow("1 owner") << true;
		QTest::newRow("2 owner") << true; // this is validated on the server side; client trusts everything
		QTest::newRow("1 useracl") << true;
		QTest::newRow("2 useracl") << false;
		QTest::newRow("1 layeracl") << true;
		QTest::newRow("2 layeracl") << false;
		QTest::newRow("1 featureaccess") << true;
		QTest::newRow("2 featureaccess") << false;
		QTest::newRow("1 defaultlayer") << true;
		QTest::newRow("2 defaultlayer") << false;
		QTest::newRow("1 resize") << true;
		QTest::newRow("2 resize") << false;

		// Other messages can be sent by everyone by default
		QTest::newRow("2 chat") << true;
		QTest::newRow("2 laser") << true;
		QTest::newRow("2 movepointer") << true;
		QTest::newRow("2 undopoint") << true;
		QTest::newRow("2 putimage img=000") << true;
		QTest::newRow("2 fillrect") << true;
		QTest::newRow("2 penup") << true;
		QTest::newRow("2 moveregion") << true;

		QTest::newRow("2 newannotation id=0x0201") << true;
		QTest::newRow("2 reshapeannotation") << true;
		QTest::newRow("2 editannotation") << true;
		QTest::newRow("2 deleteannotation") << true;

		// Test annotation ID validation
		// OPs can pick any ID, but other users must prefix the ID with their context ID
		QTest::newRow("1 newannotation id=0x0101") << true;
		QTest::newRow("1 newannotation id=0x0201") << true;
		QTest::newRow("2 newannotation id=0x0101") << false;
		QTest::newRow("2 newannotation id=0x0201") << true;
	}

	void testFilterInitialStateLocal()
	{
		QFETCH(bool, allowed);

		AclFilter acl;
		acl.reset(1, true);

		QCOMPARE(acl.filterMessage(*msg(QTest::currentDataTag())), allowed);
	}

	void testOwnLayers()
	{
		AclFilter acl;
		acl.reset(1, true);

		// Non-op: Can't create layers when layerctrl lock is set
		QCOMPARE(acl.filterMessage(*msg("1 featureaccess editlayers=op")), true);
		QCOMPARE(acl.filterMessage(*msg("2 newlayer id=0x0201")), false);

		// Can create layers when ownlayers is set
		QCOMPARE(acl.filterMessage(*msg("1 featureaccess editlayers=op ownlayers=guest")), true);
		QCOMPARE(acl.filterMessage(*msg("2 newlayer id=0x0201")), true);

		// But can only edit one's own layers
		QCOMPARE(acl.filterMessage(*msg("2 layerattr id=0x0201")), true);
		QCOMPARE(acl.filterMessage(*msg("2 layerattr id=0x0101")), false);
		QCOMPARE(acl.filterMessage(*msg("2 retitlelayer id=0x0201")), true);
		QCOMPARE(acl.filterMessage(*msg("2 retitlelayer id=0x0101")), false);
		QCOMPARE(acl.filterMessage(*msg("2 deletelayer id=0x0201")), true);
		QCOMPARE(acl.filterMessage(*msg("2 deletelayer id=0x0101")), false);
	}

	void testOwnershipChange()
	{
		AclFilter acl;
		acl.reset(1, false);

		QCOMPARE(acl.filterMessage(*msg("0 owner users=1")), true);

		// User 1 is now OP: can add more ops
		QCOMPARE(acl.isLocalUserOperator(), true);
		QCOMPARE(acl.filterMessage(*msg("1 owner users=1,2")), true);
		QCOMPARE(acl.isLocalUserOperator(), true);

		// Other ops can be deopeed, though
		QCOMPARE(acl.filterMessage(*msg("2 owner users=2")), true);
		QCOMPARE(acl.isLocalUserOperator(), false);
	}

private:
	MessagePtr msg(const QString &line)
	{
		text::Parser p;
		text::Parser::Result r = p.parseLine(line);
		if(r.status != text::Parser::Result::Ok || !r.msg)
			qFatal("invalid message: %s", qPrintable(line));
		return MessagePtr(r.msg);
	}
};


QTEST_MAIN(TestAclFilter)
#include "aclfilter.moc"

