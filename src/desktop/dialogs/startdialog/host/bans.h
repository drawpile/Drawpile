// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_STARTDIALOG_HOST_BANS_H
#define DESKTOP_DIALOGS_STARTDIALOG_HOST_BANS_H
#include <QJsonArray>
#include <QJsonObject>
#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;

namespace dialogs {
namespace startdialog {
namespace host {

class Bans : public QWidget {
	Q_OBJECT
public:
	explicit Bans(QWidget *parent = nullptr);

	void reset(bool replaceBans);
	void load(const QJsonObject &json, bool replaceBans);
	QJsonObject save() const;

	void host(QStringList &outBans);

private:
	void removeSelectedBans();
	void updateButton();
	void updateBanListVisibility();
	void loadBanList(const QJsonArray &bans);
	void saveBanList();
	QJsonArray banListToJson() const;

	QLabel *m_noBansLabel;
	QWidget *m_banListWidget;
	QListWidget *m_banListView;
	QPushButton *m_removeButton;
};

}
}
}

#endif
