// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/userinfodialog.h"
#include "libclient/canvas/userlist.h"
#include "libclient/utils/kis_cubic_curve.h"
#include "ui_userinfodialog.h"
#include <QJsonObject>
#include <QPushButton>
#include <QTimer>

namespace dialogs {

struct UserInfoDialog::Private {
	int userId;
	QPushButton *refreshButton;
	Ui::UserInfoDialog ui;
};

UserInfoDialog::UserInfoDialog(const canvas::User &user, QWidget *parent)
	: QDialog{parent}
	, d{new Private}
{
	d->userId = user.id;
	d->ui.setupUi(this);

	d->ui.nameValue->setText(user.name);
	d->ui.userIdValue->setText(QString::number(user.id));
	d->ui.pressureCurveValue->setReadOnly(true);

	d->refreshButton = new QPushButton{this};
	d->refreshButton->setText(tr("Refresh"));
	d->ui.buttons->addButton(d->refreshButton, QDialogButtonBox::ActionRole);

	connect(
		d->refreshButton, &QPushButton::pressed, this,
		&UserInfoDialog::triggerUpdate);
	connect(d->ui.buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
	connect(d->ui.buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

UserInfoDialog::~UserInfoDialog()
{
	delete d;
}

int UserInfoDialog::userId() const
{
	return d->userId;
}

void UserInfoDialog::triggerUpdate()
{
	if(d->refreshButton->isEnabled()) {
		d->refreshButton->setEnabled(false);
		updateInfo(QJsonObject{});
		QTimer::singleShot(500, this, &UserInfoDialog::emitRequestUserInfo);
	}
}

void UserInfoDialog::receiveUserInfo(int userId, const QJsonObject &info)
{
	if(userId == d->userId) {
		d->refreshButton->setEnabled(true);
		updateInfo(info);
	}
}

void UserInfoDialog::emitRequestUserInfo()
{
	emit requestUserInfo(d->userId);
}

void UserInfoDialog::updateInfo(const QJsonObject &info)
{
	d->ui.appVersionValue->setText(
		getInfoValue(info, "app_version", tr("Unknown")));
	d->ui.protocolVersionValue->setText(
		getInfoValue(info, "protocol_version", tr("Unknown")));
	d->ui.qtVersionValue->setText(
		getInfoValue(info, "qt_version", tr("Unknown")));
	d->ui.osValue->setText(getInfoValue(info, "os", tr("Unknown")));
	d->ui.tabletInputValue->setText(
		getInfoValue(info, "tablet_input", tr("Unknown")));

	QString tabletModeText;
	QString touchModeText;
	QString smoothingText;
	if(!info.isEmpty()) {
		QString touchMode = info["touch_mode"].toString();
		if(touchMode == "draw") {
			touchModeText = tr("Draw");
		} else if(touchMode == "scroll") {
			touchModeText = tr("Pan Canvas");
		} else if(touchMode == "none") {
			touchModeText = tr("Do Nothing");
		} else {
			touchModeText = tr("Unknown");
		}

		QString tabletMode = info["tablet_mode"].toString();
		if(tabletMode == "pressure") {
			tabletModeText = tr("Enabled");
		} else if(tabletMode == "none") {
			tabletModeText = tr("Disabled");
		} else {
			tabletModeText = tr("Unknown");
		}

		int smoothing = info["smoothing"].toInt(-1);
		if(smoothing >= 0) {
			smoothingText = QString::number(smoothing);
		} else {
			smoothingText = tr("Unknown");
		}
	}

	d->ui.tabletModeValue->setText(tabletModeText);
	d->ui.touchModeValue->setText(touchModeText);
	d->ui.smoothingValue->setText(smoothingText);

	QString pressureCurveString = info["pressure_curve"].toString();
	if(pressureCurveString.isEmpty()) {
		d->ui.pressureCurveValue->setVisible(false);
	} else {
		d->ui.pressureCurveValue->setVisible(true);
		KisCubicCurve curve;
		curve.fromString(pressureCurveString);
		d->ui.pressureCurveValue->setCurve(curve);
	}
}

QString UserInfoDialog::getInfoValue(
	const QJsonObject &info, const QString &key, const QString &defaultValue)
{
	if(info.isEmpty()) {
		return QString{};
	} else {
		return info[key].toString(defaultValue);
	}
}

}
