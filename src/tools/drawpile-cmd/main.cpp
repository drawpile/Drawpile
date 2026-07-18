// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>

extern "C" int drawpile_cmd_main(void);

int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);
	return drawpile_cmd_main();
}
