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
#ifndef CERTIFICATEVIEW_H
#define CERTIFICATEVIEW_H

#include <QDialog>

class Ui_CertificateView;
class QSslCertificate;

namespace dialogs {

class CertificateView final : public QDialog
{
	Q_OBJECT
public:
	CertificateView(const QString &hostname, const QSslCertificate &certificate, QWidget *parent = nullptr);
	~CertificateView() override;

private:
	Ui_CertificateView *_ui;
};

}

#endif // CERTIFICATEVIEW_H
