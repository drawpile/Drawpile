/*
   DrawPile - a collaborative drawing program.

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

#include "certificateview.h"
#include "ui_certificateview.h"

#include <QSslCertificate>

namespace dialogs {

CertificateView::CertificateView(const QString &hostname, const QSslCertificate &certificate, QWidget *parent) :
	QDialog(parent)
{
	_ui = new Ui_CertificateView;
	_ui->setupUi(this);
	setWindowTitle(tr("SSL certificate for %1").arg(hostname));
	_ui->certificateText->setText(certificate.toText());
}

CertificateView::~CertificateView()
{
	delete _ui;
}

}
