// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DESKTOP_DIALOGS_STARTDIALOG_UPDATENOTICE_H
#define DESKTOP_DIALOGS_STARTDIALOG_UPDATENOTICE_H

#include "libclient/utils/news.h"
#include <QWidget>

class QLabel;
class QPushButton;

namespace dialogs {
namespace startdialog {

class UpdateNotice final : public QWidget {
	Q_OBJECT
public:
	UpdateNotice(QWidget *parent = nullptr);

public slots:
	void setUpdate(const utils::News::Update *update);

private slots:
	void openUpdateUrl();

private:
	const utils::News::Update *m_update;
	QLabel *m_label;
	QPushButton *m_updateButton;
};

}
}

#endif
