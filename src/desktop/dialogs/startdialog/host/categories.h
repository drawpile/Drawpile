// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_STARTDIALOG_HOST_CATEGORIES_H
#define DESKTOP_DIALOGS_STARTDIALOG_HOST_CATEGORIES_H
#include <QButtonGroup>
#include <QWidget>
#include <array>

class QCheckBox;
class QRadioButton;
class QLabel;

namespace dialogs {
namespace startdialog {
namespace host {

class Categories : public QWidget {
public:
	enum Category { Session, Listing, Permissions, Roles, Bans, Count };
	using CategoryBoxes = std::array<QCheckBox *, int(Count)>;

	explicit Categories(
		const QString &text, bool loadAnnouncements = false,
		bool loadAuth = false, bool loadBans = false,
		QWidget *parent = nullptr);

	CategoryBoxes boxes() { return m_boxes; }
	QCheckBox *box(int category) { return m_boxes[category]; }
	QCheckBox *sessionBox() { return box(int(Session)); }
	QCheckBox *listingBox() { return box(int(Listing)); }
	QCheckBox *permissionsBox() { return box(int(Permissions)); }
	QCheckBox *rolesBox() { return box(int(Roles)); }
	QCheckBox *bansBox() { return box(int(Bans)); }
	QCheckBox *replaceAnnouncementsBox() { return m_replaceAnnouncementsBox; }
	QCheckBox *replaceAuthBox() { return m_replaceAuthBox; }
	QCheckBox *replaceBansBox() { return m_replaceBansBox; }

	bool isCategoryChecked(int category) const;
	bool isAnyCategoryChecked() const;
	bool isReplaceAnnouncementsChecked() const;
	bool isReplaceAuthChecked() const;
	bool isReplaceBansChecked() const;

	static QString getCategoryName(int category);

private:
	static bool isChecked(const QCheckBox *box);

	QLabel *m_label;
	CategoryBoxes m_boxes;
	QCheckBox *m_replaceAnnouncementsBox;
	QCheckBox *m_replaceAuthBox;
	QCheckBox *m_replaceBansBox;
};

}
}
}

#endif
