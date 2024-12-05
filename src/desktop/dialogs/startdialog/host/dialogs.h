// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_STARTDIALOG_HOST_IMPEXDIALOG_H
#define DESKTOP_DIALOGS_STARTDIALOG_HOST_IMPEXDIALOG_H
#include "desktop/dialogs/startdialog/host/categories.h"
#include <QDialog>
#include <QJsonObject>

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QVBoxLayout;

namespace utils {
class HostPresetModel;
}

namespace dialogs {
namespace startdialog {
namespace host {

class ResetDialog : public QDialog {
	Q_OBJECT
public:
	explicit ResetDialog(QWidget *parent = nullptr);

signals:
	void resetRequested(
		int category, bool replaceAnnouncements, bool replaceAuth,
		bool replaceBans);

private:
	void updateOkButton();
	void emitResetRequests();

	Categories *m_categories;
	QDialogButtonBox *m_buttons;
};

class LoadDialog : public QDialog {
	Q_OBJECT
public:
	explicit LoadDialog(QWidget *parent = nullptr);

signals:
	void loadRequested(
		int category, const QJsonObject &json, bool replaceAnnouncements,
		bool replaceAuth, bool replaceBans);
	void resetRequested(
		int category, bool replaceAnnouncements, bool replaceAuth,
		bool replaceBans);

private:
	void updateCurrentCategories();
	void updateCategories(int i);
	void updateButtons();
	void renameCurrentPreset();
	void deleteCurrentPreset();
	void emitLoadRequests();
	void emitLoadRequestsFrom(const QJsonObject &json);

	utils::HostPresetModel *m_presetModel;
	QComboBox *m_presetCombo;
	QPushButton *m_renameButton;
	QPushButton *m_deleteButton;
	Categories *m_categories;
	QDialogButtonBox *m_buttons;
};

class SaveDialog : public QDialog {
	Q_OBJECT
public:
	explicit SaveDialog(
		const QJsonObject &session, const QJsonObject &listing,
		const QJsonObject &permissions, const QJsonObject &roles,
		const QJsonObject &bans, QWidget *parent = nullptr);

	void accept() override;

private:
	void loadExistingTitles();
	void updateOkButton();

	std::array<QJsonObject, int(Categories::Count)> m_categoryData;
	QComboBox *m_titleCombo;
	Categories *m_categories;
	QDialogButtonBox *m_buttons;
};

class ImportDialog : public QDialog {
	Q_OBJECT
public:
	explicit ImportDialog(
		const QJsonObject &importData, QWidget *parent = nullptr);

signals:
	void loadRequested(
		int category, const QJsonObject &json, bool replaceAnnouncements,
		bool replaceAuth, bool replaceBans);

private:
	void updateButtons();
	void emitLoadRequests();

	QJsonObject m_importData;
	Categories *m_categories;
	QDialogButtonBox *m_buttons;
};

class ExportDialog : public QDialog {
	Q_OBJECT
public:
	explicit ExportDialog(
		const QJsonObject &session, const QJsonObject &listing,
		const QJsonObject &permissions, const QJsonObject &roles,
		const QJsonObject &bans, QWidget *parent = nullptr);

	void accept() override;

private:
	void updateOkButton();

	std::array<QJsonObject, int(Categories::Count)> m_categoryData;
	Categories *m_categories;
	QDialogButtonBox *m_buttons;
};

}
}
}

#endif
