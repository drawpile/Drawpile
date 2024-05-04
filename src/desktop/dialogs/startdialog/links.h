// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_STARTDIALOG_Links_H
#define DESKTOP_DIALOGS_STARTDIALOG_Links_H
#include <QWidget>

class QAbstractButton;

namespace dialogs {
namespace startdialog {

class Links final : public QWidget {
	Q_OBJECT
public:
	Links(bool vertical, QWidget *parent = nullptr);

private:
	struct LinkDefinition;

	void setUpLink(const LinkDefinition &ld, QAbstractButton *link);
};

}
}

#endif
