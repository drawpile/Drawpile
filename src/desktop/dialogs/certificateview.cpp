// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/certificateview.h"
#include "ui_certificateview.h"

#include <QSslCertificate>

namespace dialogs {

namespace {

	QString first(const QStringList &sl)
	{
		if(sl.isEmpty())
			return QStringLiteral("(not set)");
		return sl.at(0);
	}
}

CertificateView::CertificateView(const QString &hostname, const QSslCertificate &certificate, QWidget *parent) :
	QDialog(parent)
{
	_ui = new Ui_CertificateView;
	_ui->setupUi(this);
	setWindowTitle(tr("SSL Certificate for %1").arg(hostname));

	_ui->cn_label->setText(first(certificate.subjectInfo(QSslCertificate::CommonName)));
	_ui->org_label->setText(first(certificate.subjectInfo(QSslCertificate::Organization)));
	_ui->orgunit_label->setText(first(certificate.subjectInfo(QSslCertificate::OrganizationalUnitName)));
	_ui->sn_label->setText(certificate.serialNumber());

	_ui->icn_label->setText(first(certificate.issuerInfo(QSslCertificate::CommonName)));
	_ui->io_label->setText(first(certificate.issuerInfo(QSslCertificate::Organization)));
	_ui->iou_label->setText(first(certificate.issuerInfo(QSslCertificate::OrganizationalUnitName)));

	_ui->issuedon_label->setText(certificate.effectiveDate().toString());
	_ui->expireson_label->setText(certificate.expiryDate().toString());

	_ui->md5_label->setText(certificate.digest(QCryptographicHash::Md5).toHex());
	_ui->sha1_label->setText(certificate.digest(QCryptographicHash::Sha1).toHex());
	_ui->certificateText->setText(certificate.toText());
}

CertificateView::~CertificateView()
{
	delete _ui;
}

}
