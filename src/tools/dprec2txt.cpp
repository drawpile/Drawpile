/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#include <QScopedPointer>
#include <QRegularExpression>
#include <QCommandLineParser>
#include <QTextStream>

#include "config.h"

#include "txtmsg.h"

#include "../shared/record/reader.h"
#include "../shared/record/util.h"

using namespace recording;

void printVersion()
{
	printf("dprec2txt " DRAWPILE_VERSION "\n");
	printf("Protocol version: %d.%d\n", DRAWPILE_PROTO_MAJOR_VERSION, DRAWPILE_PROTO_MINOR_VERSION);
	printf("Qt version: %s (compiled against %s)\n", qVersion(), QT_VERSION_STR);
}

bool convertRecording(const QString &inputfilename, bool printMeta, QTextStream &out)
{
	Reader reader(inputfilename);
	Compatibility compat = reader.open();

	switch(compat) {
	case INCOMPATIBLE:
		fprintf(
			stderr,
			"This recording is incompatible (format version %s). It was made with Drawpile version %s.\n",
			reader.formatVersion().asString().toLocal8Bit().constData(),
			reader.writerVersion().toLocal8Bit().constData()
		);
		return false;
	case NOT_DPREC:
		fprintf(stderr, "Input file is not a Drawpile recording!\n");
		return false;
	case CANNOT_READ:
		fprintf(stderr, "Unable to read input file: %s\n", reader.errorString().toLocal8Bit().constData());
		return false;

	case COMPATIBLE:
	case MINOR_INCOMPATIBILITY:
	case UNKNOWN_COMPATIBILITY:
		// OK to proceed
		break;
	}

	// Print some header comments
	out << "## dprec2txt " DRAWPILE_VERSION ": " << inputfilename << "\n";
	out << "## Format version " << reader.formatVersion().asString() << " (Drawpile version " << reader.writerVersion() << ")\n";

	protocol::MessageType lastType = protocol::MSG_COMMAND;

	// Print messages
	bool notEof = true;
	do {
		MessageRecord mr = reader.readNext();
		switch(mr.status) {
		case MessageRecord::OK:

			if(printMeta || mr.message->isCommand()) {
				if(mr.message->type() != lastType)
					out << "\n";
				lastType = mr.message->type();

				messageToText(mr.message, out);
			}

			delete mr.message;
			break;

		case MessageRecord::INVALID:
			out << "# WARNING: Unrecognized message type " << uchar(mr.error.type) << " of length " << mr.error.len << " at offset 0x" << QString::number(reader.currentPosition(), 16) << "\n";
			break;

		case MessageRecord::END_OF_RECORDING:
			notEof = false;
			break;
		}
	} while(notEof);

	out << "\n";

	return true;
}

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);

	QCoreApplication::setOrganizationName("drawpile");
	QCoreApplication::setOrganizationDomain("drawpile.net");
	QCoreApplication::setApplicationName("dprec2txt");
	QCoreApplication::setApplicationVersion(DRAWPILE_VERSION);

	// Set up command line arguments
	QCommandLineParser parser;

	parser.setApplicationDescription("Convert Drawpile recordings to text format");
	parser.addHelpOption();

	// --version, -v
	QCommandLineOption versionOption(QStringList() << "v" << "version", "Displays version information.");
	parser.addOption(versionOption);

	// --meta, -m
	QCommandLineOption metaOption(QStringList() << "meta" << "m", "Include meta messages");
	parser.addOption(metaOption);

	// input file name
	parser.addPositionalArgument("input", "recording file", "<input.dprec>");

	// Parse
	parser.process(app);

	if(parser.isSet(versionOption)) {
		printVersion();
		return 0;
	}

	QStringList inputfiles = parser.positionalArguments();
	if(inputfiles.isEmpty()) {
		parser.showHelp(1);
		return 1;
	}

	QTextStream out(stdout, QIODevice::WriteOnly);
	if(!convertRecording(inputfiles.at(0), parser.isSet(metaOption), out))
		return 1;

	return 0;
}

