// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SESSIONSETTINGSDIALOG_H
#define SESSIONSETTINGSDIALOG_H
extern "C" {
#include <dpmsg/acl.h>
}
#include <QDialog>
#include <QJsonObject>

class Document;
class KisSliderSpinBox;
class QComboBox;
class QLabel;
class QStringListModel;
class QTimer;
class Ui_SessionSettingsDialog;

namespace canvas {
class CanvasModel;
}

namespace dialogs {

class SessionSettingsDialog final : public QDialog {
	Q_OBJECT
public:
	SessionSettingsDialog(Document *doc, QWidget *parent = nullptr);
	~SessionSettingsDialog() override;

	//! Is persistence available at all on this server?
	void setPersistenceEnabled(bool);

	void
	setBanImpExEnabled(bool isModerator, bool cryptEnabled, bool modEnabled);

	//! Is autoreset supported?
	void setAutoResetEnabled(bool);

	//! Is the local user authenticated/not a guest?
	void setAuthenticated(bool);

public slots:
	void bansImported(int total, int imported);
	void bansExported(const QByteArray &bans);
	void bansImpExError(const QString &message);

signals:
	void requestAnnouncement(const QString &apiUrl);
	void requestUnlisting(const QString &apiUrl);
	void joinPasswordChanged(const QString &joinPassword);
	void requestBanExport(bool plain);
	void requestBanImport(const QString &bans);
	void requestUpdateAuthList(const QJsonArray &list);

private slots:
	void onCanvasChanged(canvas::CanvasModel *);
	void onOperatorModeChanged(bool op);
	void onFeatureTiersChanged(const DP_FeatureTiers &features);

	void permissionPresetSaving(const QString &);
	void permissionPresetSelected(const QString &);

	void titleChanged(const QString &newTitle);

	void maxUsersChanged();
	void denyJoinsChanged(bool);
	void authOnlyChanged(bool);
	void allowWebChanged(bool allowWeb);

	void permissionChanged();
	void limitChanged();
	void updateBrushSizeLimitText(int value);

	void autoresetThresholdChanged();
	void keepChatChanged(bool);
	void persistenceChanged(bool);
	void nsfmChanged(bool);
	void deputiesChanged(int);
	void idleOverrideChanged(bool idleTimeoutOverride);

	void changePassword();
	void changeOpword();

	void changeSessionConf(
		const QString &key, const QJsonValue &value, bool now = false);
	void sendSessionConf();

	void updatePasswordLabel(QLabel *label);
	void updateAllowWebCheckbox(bool allowWeb, bool canAlter);
	void updateNsfmCheckbox(bool);
	void updateIdleSettings(int timeLimit, bool overridden, bool canOverride);
	void updateAuthListCheckboxes();

	void importBans();
	void exportBans();

	void authListChangeOp(bool op);
	void authListChangeTrusted(bool trusted);
	void importAuthList();
	void exportAuthList();

protected:
	void showEvent(QShowEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

private:
	class AuthListImport;

	static const QByteArray authExportPrefix;

	void initPermissionComboBoxes();
	void initPermissionLimitSliders();
	void updateBanImportExportState();
	void reloadSettings();
	QComboBox *featureBox(DP_Feature f);
	KisSliderSpinBox *limitSlider(DP_FeatureLimit fl);
	int limitSliderValue(DP_FeatureLimit fl);
	void setLimitSliderValue(DP_FeatureLimit fl, int value);
	bool checkBanImport(const QString &bans, QString &outErrorMessage) const;
	bool
	readAuthListImport(const QByteArray &content, QJsonArray &outList) const;
	void importAuthListChunked(const QJsonArray &list);
	void startBanExport(bool plain);
	QModelIndex getSelectedAuthListEntry();
	void authListChangeParam(const QString &key, bool value);

	Ui_SessionSettingsDialog *m_ui;
	Document *m_doc;
	QTimer *m_saveTimer;

	QJsonObject m_sessionconf;
	bool m_featureTiersChanged = false;
	bool m_featureLimitsChanged = false;

	bool m_op = false;
	bool m_isAuth = false;
	bool m_canPersist = false;
	bool m_canAlterAllowWeb = false;
	bool m_canCryptImpExBans = false;
	bool m_canModImportBans = false;
	bool m_canModExportBans = false;
	bool m_canAutoreset = false;
	bool m_awaitingImportExportResponse = false;
	QString m_importExportTitle;
};

}

#endif
