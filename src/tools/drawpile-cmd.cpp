/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#include "renderer.h"
#include "../libshared/net/protover.h"

#include <QGuiApplication>
#include <QStringList>
#include <QRegularExpression>
#include <QCommandLineParser>
#include <QImageWriter>
#include <QFileInfo>
#include <QDir>

void printVersion()
{
	printf("drawpile-cmd " DRAWPILE_VERSION "\n");
	printf("Protocol version: %s\n", qPrintable(protocol::ProtocolVersion::current().asString()));
	printf("Qt version: %s (compiled against %s)\n", qVersion(), QT_VERSION_STR);
}

int main(int argc, char *argv[]) {
	// Force the use of offscreen platform, so this can be used headlessly.
	qputenv("QT_QPA_PLATFORM", "offscreen");

	QGuiApplication app(argc, argv);

	QGuiApplication::setOrganizationName("drawpile");
	QGuiApplication::setOrganizationDomain("drawpile.net");
	QGuiApplication::setApplicationName("drawpile-cmd");
	QGuiApplication::setApplicationVersion(DRAWPILE_VERSION);

	// Set up command line arguments
	QCommandLineParser parser;

	parser.setApplicationDescription("A commandline tool for rendering Drawpile recordings");
	parser.addHelpOption();

	// --version, -v
	QCommandLineOption versionOption(QStringList() << "v" << "version", "Displays version information.");
	parser.addOption(versionOption);

	// --verbose, -V
	QCommandLineOption verboseOption(QStringList() << "V" << "verbose", "Print extra debugging information");
	parser.addOption(verboseOption);

	// --out, -o
	QCommandLineOption outOption(QStringList() << "o" << "out", "Output file pattern (use - to output to stdout)", "output");
	parser.addOption(outOption);

	// --format, -f
	QCommandLineOption formatOption(QStringList() << "f" << "format", "Output file format", "fmt");
	parser.addOption(formatOption);

	// --merge-annotations, -a
	QCommandLineOption mergeAnnotationsOption(QStringList() << "a" << "merge-annotations", "Include annotations in saved images");
	parser.addOption(mergeAnnotationsOption);

	// --every-seq, -e <n>
	QCommandLineOption everySeqOption(QStringList() << "e" << "every-seq", "Export image every n sequence", "n");
	parser.addOption(everySeqOption);

	// --every-msg, -m
	QCommandLineOption everyMsgOption(QStringList() << "m" << "every-msg", "Export image every n messages", "n");
	parser.addOption(everyMsgOption);

	// input file name
	parser.addPositionalArgument("input", "recording file", "<input.dprec>");

	// --acl, -A
	QCommandLineOption aclOption(QStringList() << "A" << "acl", "Perform ACL filtering");
	parser.addOption(aclOption);

	// --maxsize, -s
	QCommandLineOption maxSizeOption(QStringList() << "s" << "maxsize", "Maximum exported image dimensions", "size");
	parser.addOption(maxSizeOption);

	// --fixedsize, -S
	QCommandLineOption fixedSizeOption(QStringList() << "S" << "fixedsize", "Make all images the same size (maxsize if set)");
	parser.addOption(fixedSizeOption);

	// Parse
	parser.process(app);

	if(parser.isSet(versionOption)) {
		printVersion();
		return 0;
	}

	const QStringList inputfiles = parser.positionalArguments();
	if(inputfiles.size() != 1) {
		parser.showHelp(1);
		return 1;
	}

	if(parser.isSet(everySeqOption) && parser.isSet(everyMsgOption)) {
		fprintf(stderr, "--every-msg and --every-seq are mutually exclusive\n");
		return 1;
	}

	int exportEvery = 0;
	ExportEvery exportEveryMode = ExportEvery::Sequence;
	if(parser.isSet(everySeqOption)) {
		exportEvery = parser.value(everySeqOption).toInt();
	} else {
		exportEvery = parser.value(everyMsgOption).toInt();
		exportEveryMode = ExportEvery::Message;
	}

	if(exportEvery < 0) {
		fprintf(stderr, "Export every n: negative values not allowed.\n");
		return 1;
	}

	QSize maxSize;
	if(parser.isSet(maxSizeOption)) {
		QRegularExpression re("^(\\d+)[xX0](\\d+)$");
		auto m = re.match(parser.value(maxSizeOption));
		if(!m.hasMatch()) {
			fprintf(stderr, "Size must be given in format WIDTHxHEIGHT\n");
			return 1;
		}
		maxSize = QSize(m.captured(1).toInt(), m.captured(2).toInt());
		if(maxSize.isEmpty()) {
			fprintf(stderr, "Size must not be zero\n");
			return 1;
		}
	}

	const QFileInfo inputfile = inputfiles.at(0);
	QString outputFilePattern = parser.value(outOption);
	if(outputFilePattern.isEmpty()) {
		outputFilePattern = QFileInfo(inputfile.absoluteDir(), inputfile.baseName() + "-:idx:.jpeg").absoluteFilePath();
	} else {
		const QFileInfo outputfile = outputFilePattern;
		if(outputfile.isDir())
			outputFilePattern = QFileInfo(outputfile.absoluteDir(), inputfile.baseName() + "-:idx:.jpeg").absoluteFilePath();
	}

	QByteArray outputFormat = parser.value(formatOption).toLower().toUtf8();
	if(outputFormat.isEmpty()) {
		const int suffix = outputFilePattern.lastIndexOf('.');
		if(suffix < 0) {
			fprintf(stderr, "Select output file format with --format\n");
			return 1;
		}
		outputFormat = outputFilePattern.mid(suffix+1).toLower().toUtf8();
	}

	if(!QImageWriter::supportedImageFormats().contains(outputFormat) && outputFormat != "ora") {
		fprintf(stderr, "Unsupported file format: %s\n", outputFormat.constData());
		fprintf(stderr, "Must be one of: %s, ora\n", QImageWriter::supportedImageFormats().join(", ").constData());
		return 1;
	}

	const DrawpileCmdSettings settings {
		inputfiles.at(0),
		outputFilePattern,
		outputFormat,
		exportEvery,
		exportEveryMode,
		maxSize,
		parser.isSet(fixedSizeOption),
		parser.isSet(mergeAnnotationsOption),
		parser.isSet(verboseOption),
		parser.isSet(aclOption)
	};

	return renderDrawpileRecording(settings);
}

