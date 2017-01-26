/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2017 Calle Laakkonen

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

#include "config.h"

#include "../shared/record/reader.h"
#include "../shared/record/writer.h"

#include <QCoreApplication>
#include <QStringList>
#include <QScopedPointer>
#include <QRegularExpression>
#include <QCommandLineParser>
#include <QTextStream>
#include <QFile>

using namespace recording;

void printVersion()
{
	printf("dprectool " DRAWPILE_VERSION "\n");
	printf("Protocol version: %s\n", qPrintable(protocol::ProtocolVersion::current().asString()));
	printf("Qt version: %s (compiled against %s)\n", qVersion(), QT_VERSION_STR);
}

bool convertRecording(const QString &inputfilename, const QString &outputfilename, const QString &outputFormat)
{
	// Open input file
	Reader reader(inputfilename);
	Compatibility compat = reader.open();

	switch(compat) {
	case INCOMPATIBLE:
		fprintf(
			stderr,
			"This recording is incompatible (format version %s). It was made with Drawpile version %s.\n",
			qPrintable(reader.formatVersion().asString()),
			qPrintable(reader.writerVersion())
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

	// Open output file (stdout if no filename given)
	QScopedPointer<Writer> writer;
	if(outputfilename.isEmpty()) {
		// No output filename given? Write to stdout
		QFile *out = new QFile();
		out->open(stdout, QFile::WriteOnly);
		writer.reset(new Writer(out));
		out->setParent(writer.data());

		writer->setEncoding(Writer::Encoding::Text);

	} else {
		writer.reset(new Writer(outputfilename));
	}

	// Output format override
	if(outputFormat == "text")
		writer->setEncoding(Writer::Encoding::Text);
	else if(outputFormat == "binary")
		writer->setEncoding(Writer::Encoding::Binary);
	else if(!outputFormat.isEmpty()) {
		fprintf(stderr, "Invalid output format: %s\n", qPrintable(outputFormat));
		return false;
	}

	// Convert input to output
	if(!writer->open()) {
		fprintf(stderr, "Couldn't open %s: %s\n",
			outputfilename.toLocal8Bit().constData(),
			writer->errorString().toLocal8Bit().constData()
			);
		return false;
	}
	if(!writer->writeHeader()) {
		fprintf(stderr, "Error while writing header: %s\n",
			writer->errorString().toLocal8Bit().constData()
			);
		return false;
	}

	bool notEof = true;
	do {
		MessageRecord mr = reader.readNext();
		switch(mr.status) {
		case MessageRecord::OK:

			if(!writer->writeMessage(*mr.message)) {
				fprintf(stderr, "Error while writing message: %s\n",
					writer->errorString().toLocal8Bit().constData()
					);
				return false;
			}
			delete mr.message;
			break;

		case MessageRecord::INVALID:
			writer->writeComment(QStringLiteral("WARNING: Unrecognized message type %1 of length %2 at offset 0x%3")
				.arg(int(mr.error.type))
				.arg(mr.error.len)
				.arg(reader.currentPosition())
				);
			break;

		case MessageRecord::END_OF_RECORDING:
			notEof = false;
			break;
		}
	} while(notEof);

	return true;
}

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);

	QCoreApplication::setOrganizationName("drawpile");
	QCoreApplication::setOrganizationDomain("drawpile.net");
	QCoreApplication::setApplicationName("dprectool");
	QCoreApplication::setApplicationVersion(DRAWPILE_VERSION);

	// Set up command line arguments
	QCommandLineParser parser;

	parser.setApplicationDescription("Convert Drawpile recordings between text and binary formats");
	parser.addHelpOption();

	// --version, -v
	QCommandLineOption versionOption(QStringList() << "v" << "version", "Displays version information.");
	parser.addOption(versionOption);

	// --out, -o
	QCommandLineOption outOption(QStringList() << "o" << "out", "Output file", "output");
	parser.addOption(outOption);

	// --format, -f
	QCommandLineOption formatOption(QStringList() << "f" << "format", "Output format (binary/text)", "format");
	parser.addOption(formatOption);

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

	if(!convertRecording(inputfiles.at(0), parser.value(outOption), parser.value(formatOption)))
		return 1;

	return 0;
}

