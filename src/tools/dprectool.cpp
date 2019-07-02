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

#include "config.h"
#include "stats.h"

#include "../libshared/record/reader.h"
#include "../libshared/record/writer.h"
#include "../libclient/canvas/aclfilter.h"

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

bool convertRecording(const QString &inputfilename, const QString &outputfilename, const QString &outputFormat, bool doAclFiltering)
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
		fprintf(stderr, "Unable to read input file: %s\n", qPrintable(reader.errorString()));
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

	// Open output file
	if(!writer->open()) {
		fprintf(stderr, "Couldn't open %s: %s\n",
			qPrintable(outputfilename),
			qPrintable(writer->errorString())
			);
		return false;
	}
	if(!writer->writeHeader(reader.metadata())) {
		fprintf(stderr, "Error while writing header: %s\n",
			qPrintable(writer->errorString())
			);
		return false;
	}

	// Prepare filters
	canvas::AclFilter aclFilter;
	aclFilter.reset(1, false);

	// Convert and/or filter recording
	bool notEof = true;
	do {
		MessageRecord mr = reader.readNext();
		switch(mr.status) {
		case MessageRecord::OK: {
			if(doAclFiltering && !aclFilter.filterMessage(*mr.message)) {
				writer->writeMessage(*mr.message->asFiltered());

			} else {
				if(!writer->writeMessage(*mr.message)) {
					fprintf(stderr, "Error while writing message: %s\n",
						qPrintable(writer->errorString())
						);
					return false;
				}
			}
			break;
			}

		case MessageRecord::INVALID:
			writer->writeComment(QStringLiteral("WARNING: Unrecognized message type %1 of length %2 at offset 0x%3")
				.arg(int(mr.invalid_type))
				.arg(mr.invalid_len)
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

/**
 * Print the version number of this recording. The output can be parsed easily in a shell script.
 * Output format: <compatibility flag> <protocol version> <client version string>
 * Example: C dp:4.20.1 2.0.5
 * Compatability flag is one of:
 *   - C: fully compatible with this dprectool/drawpile-cmd version
 *   - M: minor incompatibility (might render differently)
 *   - U: unknown compatibility (made with a newer version: some features may be missing)
 *   - I: known to be incompatible
 */
bool printRecordingVersion(const QString &inputFilename)
{
	Reader reader(inputFilename);
	const Compatibility compat = reader.open();

	char compatflag = '?';
	switch(compat) {
		case COMPATIBLE: compatflag = 'C'; break;
		case MINOR_INCOMPATIBILITY: compatflag = 'M'; break;
		case UNKNOWN_COMPATIBILITY: compatflag = 'U'; break;
		case INCOMPATIBLE: compatflag = 'I'; break;
		case NOT_DPREC:
			fprintf(stderr, "Not a drawpile recording!\n");
			return false;
		case CANNOT_READ:
			fprintf(stderr, "Cannot read file: %s", qPrintable(reader.errorString()));
			return false;
	}

	printf("%c %s %s\n",
		compatflag,
		qPrintable(reader.formatVersion().asString()),
		reader.writerVersion().isEmpty() ? "(no writer version)" : qPrintable(reader.writerVersion())
		);
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
	QCommandLineOption formatOption(QStringList() << "f" << "format", "Output format (binary/text/version)", "format");
	parser.addOption(formatOption);

	// --acl, -A
	QCommandLineOption aclOption(QStringList() << "A" << "acl", "Perform ACL filtering");
	parser.addOption(aclOption);

	// --msg-freq
	QCommandLineOption msgFreqOption(QStringList() << "msg-freq", "Print message frequency table");
	parser.addOption(msgFreqOption);

	// input file name
	parser.addPositionalArgument("input", "recording file", "<input.dprec>");

	// Parse
	parser.process(app);

	if(parser.isSet(versionOption)) {
		printVersion();
		return 0;
	}

	const QStringList inputfiles = parser.positionalArguments();
	if(inputfiles.isEmpty()) {
		parser.showHelp(1);
		return 1;
	}

	const QString format = parser.value(formatOption);
	if(format == "version") {
		return !printRecordingVersion(inputfiles.at(0));
	}

	if(parser.isSet(msgFreqOption)) {
		return printMessageFrequency(inputfiles.at(0)) ? 0 : 1;
	}

	if(!convertRecording(
		inputfiles.at(0),
		parser.value(outOption),
		parser.value(formatOption),
		parser.isSet(aclOption)
		))
		return 1;

	return 0;
}

