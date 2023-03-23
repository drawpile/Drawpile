// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef USERINFODIALOG_H
#define USERINFODIALOG_H
#include <QDialog>

class QJsonObject;

namespace canvas {
struct User;
}

namespace dialogs {

class UserInfoDialog final : public QDialog {
	Q_OBJECT
public:
	UserInfoDialog(const canvas::User &user, QWidget *parent = nullptr);
	~UserInfoDialog() override;

	UserInfoDialog(const UserInfoDialog &) = delete;
	UserInfoDialog(UserInfoDialog &&) = delete;
	UserInfoDialog &operator=(const UserInfoDialog &) = delete;
	UserInfoDialog &operator=(UserInfoDialog &&) = delete;

	int userId() const;

	void triggerUpdate();

public slots:
	void receiveUserInfo(int userId, const QJsonObject &info);

signals:
	void requestUserInfo(int userId);

private slots:
	void emitRequestUserInfo();

private:
	void updateInfo(const QJsonObject &info);
	QString getInfoValue(
		const QJsonObject &info, const QString &key,
		const QString &defaultValue);

	struct Private;
	Private *d;
};

}

#endif
