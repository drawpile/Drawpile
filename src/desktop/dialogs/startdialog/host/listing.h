// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_STARTDIALOG_HOST_LISTING_H
#define DESKTOP_DIALOGS_STARTDIALOG_HOST_LISTING_H
#include <QJsonObject>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListView;
class QPushButton;

namespace net {
class AnnouncementListModel;
}

namespace dialogs {
namespace startdialog {
namespace host {

class Listing : public QWidget {
	Q_OBJECT
public:
	explicit Listing(QWidget *parent = nullptr);

	void reset(bool replaceAnnouncements);
	void load(const QJsonObject &json, bool replaceAnnouncements);
	QJsonObject save() const;

	void setPersonal(bool personal);
	void startEditingTitle();

	void host(
		bool nsfmAllowed, QStringList &outErrors, QString &outTitle,
		QString &outAlias, QStringList &outAnnouncementUrls);

signals:
	void requestNsfmBasedOnListing();

private:
	static constexpr int MAX_DESCRIPTION_LENGTH = 500;

	void updateEditSectionVisibility();
	bool isEditSectionVisible() const;

	void reloadAnnouncementMenu();
	void updateRemoveAnnouncementsButton();
	void addAnnouncement(const QString &url);
	void removeSelectedAnnouncements();
	void persistAnnouncements();

	void checkNsfmTitle(const QString &title);
	void checkNsfmAlias(const QString &alias);

	QCheckBox *m_automaticBox;
	QWidget *m_editSection;
	QLineEdit *m_titleEdit;
	QLineEdit *m_idAliasEdit;
	net::AnnouncementListModel *m_announcementModel;
	QListView *m_announcements;
	QPushButton *m_addAnnouncementButton;
	QPushButton *m_removeAnnouncementButton;
	bool m_personal = true;
};

}
}
}

#endif
