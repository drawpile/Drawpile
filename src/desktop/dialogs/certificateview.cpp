/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

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
