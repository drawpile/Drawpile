// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef JOINDIALOG_H
#define JOINDIALOG_H

#include <QDialog>

class Ui_JoinDialog;

class SessionListingModel;
class SessionFilterProxyModel;

class QAction;
class QMenu;

namespace dialogs {

class JoinDialog final : public QDialog {
	Q_OBJECT
public:
	explicit JoinDialog(const QUrl &defaultUrl, QWidget *parent = nullptr);
	~JoinDialog() override;

	//! Get the host address
	QString getAddress() const;

	//! Get the join parameters encoded as an URL
	QUrl getUrl() const;

	//! Get the selected recording filename (empty if not selected)
	QString autoRecordFilename() const;

	//! Restore settings from configuration file
	void restoreSettings();

	//! Store settings in configuration file
	void rememberSettings() const;

protected:
	void resizeEvent(QResizeEvent *event) override;

private slots:
	void addressChanged(const QString &addr);
	void refreshListing();
	void recordingToggled(bool checked);

	void addListServer();

	void showListingContextMenu(const QPoint &pos);

	void setUrlFromIndex(const QModelIndex &index);
	void joinIndex(const QModelIndex &index);

private:
	void resolveRoomcode(const QString &roomcode, const QStringList &servers);
	void setListingVisible(bool show);

	void addListServerUrl(const QUrl &url);

	QAction *makeCopySessionDataAction(const QString &text, int role);

	bool canJoinIndex(const QModelIndex &index);
	bool isListingIndex(const QModelIndex &index);

	Ui_JoinDialog *m_ui;
	QPushButton *m_addServerButton;
	SessionFilterProxyModel *m_filteredSessions;
	SessionListingModel *m_sessions;
	QMenu *m_listingContextMenu;
	QAction *m_joinAction;

	qint64 m_lastRefresh;

	QString m_recordingFilename;
};

}

#endif
