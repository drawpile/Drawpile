/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016 Calle Laakkonen

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
#include <QCoreApplication>
#include <QStringList>
#include <QCommandLineParser>

#include "config.h"

#include "txtreader.h"

#include "../shared/record/writer.h"
#include "../shared/record/hibernate.h"

using namespace recording;

void printVersion()
{
	printf("txt2dprec " DRAWPILE_VERSION "\n");
	printf("Protocol version: %d.%d\n", DRAWPILE_PROTO_MAJOR_VERSION, DRAWPILE_PROTO_MINOR_VERSION);
	printf("Qt version: %s (compiled against %s)\n", qVersion(), QT_VERSION_STR);
}

bool convertRecording(const QString &infile, const QString &outfile, recording::HibernationHeader *hibernation)
{
	TextReader reader(infile);
	if(!reader.load()) {
		fprintf(stderr, "%s\n", reader.errorMessage().toLocal8Bit().constData());
		return false;
	}

	recording::Writer writer(outfile);

	if(!writer.open())
		return false;

	writer.setFilterMeta(false);

	bool ok;
	if(hibernation)
		ok = writer.writeHibernationHeader(*hibernation);
	else
		ok = writer.writeHeader();

	if(!ok) {
		fprintf(stderr, "%s\n", writer.errorString().toLocal8Bit().constData());
		return false;
	}

	for(protocol::MessagePtr msg : reader.messages())
		writer.recordMessage(msg);

	writer.close();

	return true;
}

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);

	QCoreApplication::setOrganizationName("drawpile");
	QCoreApplication::setOrganizationDomain("drawpile.net");
	QCoreApplication::setApplicationName("txt2dprec");
	QCoreApplication::setApplicationVersion(DRAWPILE_VERSION);

	// Set up command line arguments
	QCommandLineParser parser;

	parser.setApplicationDescription("Convert Drawpile text recordings to binary format");
	parser.addHelpOption();

	// --version, -v
	QCommandLineOption versionOption(QStringList() << "v" << "version", "Displays version information.");
	parser.addOption(versionOption);

	// --title, -t
	QCommandLineOption titleOption(QStringList() << "title" << "t", "Set session title (hibernated session)", "title");
	parser.addOption(titleOption);

	// --founder, -f
	QCommandLineOption founderOption(QStringList() << "founder" << "f", "Set founder name (hibernated session)", "name");
	parser.addOption(founderOption);

	// --persistent, -p
	QCommandLineOption persistentOption(QStringList() << "persistent" << "p", "Persistent session (hibernated session)");
	parser.addOption(persistentOption);


	// input file name
	parser.addPositionalArgument("input", "text file", "<input.dptxt>");

	// output file name
	parser.addPositionalArgument("output", "recording file", "<output.dprec>");

	// Parse
	parser.process(app);

	if(parser.isSet(versionOption)) {
		printVersion();
		return 0;
	}

	QStringList files = parser.positionalArguments();
	if(files.size() != 2) {
		parser.showHelp(1);
		return 1;
	}

	HibernationHeader hib;

	hib.minorVersion = DRAWPILE_PROTO_MINOR_VERSION;
	hib.title = parser.value(titleOption);
	hib.founder = parser.value(founderOption);
	hib.flags = parser.isSet(persistentOption) ? recording::HibernationHeader::PERSISTENT : recording::HibernationHeader::NOFLAGS;

	// Setting any of the hibernation properties means the hibernation
	// header must be written.
	const bool hibernate = !hib.title.isEmpty() || hib.founder.isEmpty();

	if(!convertRecording(files.at(0), files.at(1), hibernate ? &hib : nullptr))
		return 1;

	return 0;
}

