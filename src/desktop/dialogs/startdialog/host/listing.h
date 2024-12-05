// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_STARTDIALOG_HOST_LISTING_H
#define DESKTOP_DIALOGS_STARTDIALOG_HOST_LISTING_H
#include <QTextCursor>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListView;
class QPlainTextEdit;
class QPushButton;

namespace dialogs {
namespace startdialog {
namespace host {

class Listing : public QWidget {
	Q_OBJECT
public:
	explicit Listing(QWidget *parent = nullptr);

	void setPersonal(bool personal);

private:
	static constexpr int MAX_DESCRIPTION_LENGTH = 500;

	void updateEditSectionVisibility();
	void updateDescriptionLength();

	QCheckBox *m_automaticBox;
	QWidget *m_editSection;
	QLineEdit *m_titleEdit;
	QPlainTextEdit *m_descriptionEdit;
	QLabel *m_descriptionCounter;
	QListView *m_listings;
	QPushButton *m_addListingButton;
	QPushButton *m_removeListingButton;
	QString m_prevDescriptionText;
	bool m_personal = true;
	bool m_updatingDescription = false;
};

}
}
}

#endif
