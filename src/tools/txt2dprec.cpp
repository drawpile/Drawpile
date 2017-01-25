/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016-2017 Calle Laakkonen

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
#include <QFile>

#include "config.h"

#include "../shared/record/writer.h"
#include "../shared/net/textmode.h"

using namespace recording;
using protocol::text::Parser;

void printVersion()
{
	printf("txt2dprec " DRAWPILE_VERSION "\n");
	printf("Protocol version: %d.%d\n", DRAWPILE_PROTO_MAJOR_VERSION, DRAWPILE_PROTO_MINOR_VERSION);
	printf("Qt version: %s (compiled against %s)\n", qVersion(), QT_VERSION_STR);
}

bool convertRecording(const QString &infileName, const QString &outfileName)
{
	QFile infile(infileName);
	if(!infile.open(QFile::ReadOnly|QFile::Text)) {
		fprintf(stderr, "%s\n", infile.errorString().toLocal8Bit().constData());
		return false;
	}

	Parser parser;

	recording::Writer writer(outfileName);

	if(!writer.open())
		return false;

	bool ok;
	ok = writer.writeHeader();

	if(!ok) {
		fprintf(stderr, "%s\n", writer.errorString().toLocal8Bit().constData());
		return false;
	}

	int lineNum = 0;
	for(;;) {
		++lineNum;
		QByteArray bline = infile.readLine();
		if(bline.isEmpty())
			break;

		QString line = QString::fromUtf8(bline.trimmed());

		if(line.isEmpty() || line.at(0) == '#')
			continue;

		Parser::Result r = parser.parseLine(line);
		switch(r.status) {
		case Parser::Result::Ok:
			writer.writeMessage(*r.msg);
			delete r.msg;
			break;
		case Parser::Result::Skip:
		case Parser::Result::NeedMore:
			break;
		case Parser::Result::Error:
			fprintf(stderr, "%s:%d: %s\n", infileName.toLocal8Bit().constData(), lineNum, parser.errorString().toLocal8Bit().constData());
			break;
		}

	}

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

	if(!convertRecording(files.at(0), files.at(1)))
		return 1;

	return 0;
}

