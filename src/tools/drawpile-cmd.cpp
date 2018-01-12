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
#include "../shared/net/protover.h"

#include <QGuiApplication>
#include <QStringList>
#include <QRegularExpression>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QDir>

void printVersion()
{
	printf("drawpile-cmd " DRAWPILE_VERSION "\n");
	printf("Protocol version: %s\n", qPrintable(protocol::ProtocolVersion::current().asString()));
	printf("Qt version: %s (compiled against %s)\n", qVersion(), QT_VERSION_STR);
}

int main(int argc, char *argv[]) {
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
	QCommandLineOption outOption(QStringList() << "o" << "out", "Output file pattern", "output");
	parser.addOption(outOption);

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

	const QFileInfo inputfile = inputfiles.at(0);
	QString outputFilePattern = parser.value(outOption);
	if(outputFilePattern.isEmpty()) {
		outputFilePattern = QFileInfo(inputfile.absoluteDir(), inputfile.baseName() + "-:idx:.jpeg").absoluteFilePath();
	} else {
		const QFileInfo outputfile = outputFilePattern;
		if(outputfile.isDir())
			outputFilePattern = QFileInfo(outputfile.absoluteDir(), inputfile.baseName() + "-:idx:.jpeg").absoluteFilePath();
	}

	const DrawpileCmdSettings settings {
		inputfiles.at(0),
		outputFilePattern,
		exportEvery,
		exportEveryMode,
		parser.isSet(mergeAnnotationsOption),
		parser.isSet(verboseOption),
		parser.isSet(aclOption)
	};

	return renderDrawpileRecording(settings);
}

