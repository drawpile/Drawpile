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
	static constexpr int DONATION_LINK_INDEX = 0;
	struct LinkDefinition;

	void setUpLink(int index, const LinkDefinition &ld, QAbstractButton *link);
};

}
}

#endif
