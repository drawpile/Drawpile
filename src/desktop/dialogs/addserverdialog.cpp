/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2020 Calle Laakkonen

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

#include "desktop/dialogs/addserverdialog.h"
#include "libshared/util/networkaccess.h"
#include "libclient/utils/listservermodel.h"

#include <QIcon>
#include <QPushButton>

namespace dialogs {

AddServerDialog::AddServerDialog(QWidget *parent)
	 : QMessageBox(
		   QMessageBox::Icon::NoIcon,
		   AddServerDialog::tr("Add Server"),
		   QString(),
		   QMessageBox::Cancel,
		   parent),
	   m_servers(nullptr)
{
	setAttribute(Qt::WA_DeleteOnClose);
}

void AddServerDialog::setListServerModel(sessionlisting::ListServerModel *model)
{
	m_servers = model;
}

void AddServerDialog::query(const QUrl &url)
{
	auto *response = sessionlisting::getApiInfo(url);
	connect(response, &sessionlisting::AnnouncementApiResponse::finished, this, [this, response](const QVariant &result, const QString&, const QString &error) {
		if(error.isEmpty()) {
			m_serverInfo = result.value<sessionlisting::ListServerInfo>();
			m_url = response->apiUrl();
			showSuccess();
		} else {
			showError(error);
		}
		response->deleteLater();
	});
}

void AddServerDialog::showError(const QString &errorMessage)
{
	setIcon(Icon::Warning);
	setText(errorMessage);
	show();
}

void AddServerDialog::showSuccess()
{
	setText(QStringLiteral("<b>%1</b><br><br>%2").arg(m_serverInfo.name.toHtmlEscaped(), m_serverInfo.description.toHtmlEscaped()));
	auto *btn = addButton(tr("Add"), AcceptRole);
	connect(btn, &QPushButton::clicked, this, &AddServerDialog::onAddClicked);
	show();

	if(m_serverInfo.faviconUrl == "drawpile") {
		const auto icon = QIcon(":/icons/dancepile.png").pixmap(128, 128);
		m_favicon = icon.toImage();
		setIconPixmap(icon);


	} else {
		const QUrl faviconUrl(m_serverInfo.faviconUrl);
		if(faviconUrl.isValid()) {
			auto *filedownload = new networkaccess::FileDownload(this);

			filedownload->setExpectedType("image/");

			connect(filedownload, &networkaccess::FileDownload::finished, this, [filedownload, this](const QString &errorMessage) {
				filedownload->deleteLater();
				if(!errorMessage.isEmpty()) {
					qWarning("Couldnt' fetch favicon: %s", qPrintable(errorMessage));
					return;
				}

				if(!m_favicon.load(filedownload->file(), nullptr)) {
					qWarning("Couldn't load favicon.");
					return;
				}

				setIconPixmap(QPixmap::fromImage(m_favicon));
			});

			filedownload->start(faviconUrl);
		}
	}
}

void AddServerDialog::onAddClicked()
{
	sessionlisting::ListServerModel *listservers;
	bool autosave;
	if(m_servers) {
		listservers = m_servers;
		autosave = false;
	} else {
		listservers = new sessionlisting::ListServerModel(true, this);
		autosave = true;
	}

	const auto url = m_url.toString();

	listservers->addServer(
		m_serverInfo.name,
		url,
		m_serverInfo.description,
		m_serverInfo.readOnly,
		m_serverInfo.publicListings,
		m_serverInfo.privateListings
	);

	if(!m_favicon.isNull()) {
		listservers->setFavicon(
			url,
			QIcon(":/icons/dancepile.png").pixmap(128, 128).toImage()
			);
	}

	if(autosave)
		listservers->saveServers();

	emit serverAdded(m_serverInfo.name);
}

}
