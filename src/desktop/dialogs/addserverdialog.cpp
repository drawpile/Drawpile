// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/dialogs/addserverdialog.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/utils/listservermodel.h"
#include "libshared/util/networkaccess.h"
#include "ui_addserverdialog.h"

#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QPushButton>
#include <QStyle>
#include <QTextBrowser>

namespace dialogs {

class FaviconTextBrowser : public QTextBrowser {
public:
	FaviconTextBrowser(QImage &favicon)
		: QTextBrowser{}
		, m_favicon(favicon)
	{
	}

protected:
	QVariant loadResource(int type, const QUrl &name) override
	{
		if(type == QTextDocument::ImageResource && name == QUrl{"favicon"}) {
			return m_favicon;
		} else {
			return QTextBrowser::loadResource(type, name);
		}
	}

private:
	QImage &m_favicon;
};

struct AddServerDialog::Private {
	Ui::AddServerDialog ui;
	FaviconTextBrowser *browser;
	QAbstractButton *okButton;
	QAbstractButton *cancelButton;
	bool httpFallbackAttempted = false;
	bool cancelRequested = false;
	QUrl requestUrl;
	sessionlisting::AnnouncementApiResponse *apiResponse = nullptr;
	networkaccess::FileDownload *faviconDownload = nullptr;
	sessionlisting::ListServerModel *listServerModel = nullptr;
	QUrl url;
	QImage favicon;
	sessionlisting::ListServerInfo serverInfo;
};


AddServerDialog::AddServerDialog(QWidget *parent)
	: QDialog{parent}
	, d{new Private}
{
	d->ui.setupUi(this);
	d->browser = new FaviconTextBrowser{d->favicon};
	utils::bindKineticScrolling(d->browser);
	d->ui.resultLayout->addWidget(d->browser);
	d->okButton = d->ui.buttons->button(QDialogButtonBox::Ok);
	d->cancelButton = d->ui.buttons->button(QDialogButtonBox::Cancel);
	d->ui.urlEdit->setInputMethodHints(Qt::ImhUrlCharactersOnly);
	d->ui.errorLabel->hide();

	connect(
		d->ui.urlEdit, &QLineEdit::textChanged, this,
		&AddServerDialog::updateCheckButton);
	connect(
		d->ui.buttons, &QDialogButtonBox::clicked, this,
		&AddServerDialog::handleButtonClick);

	setPage(URL_PAGE);
}

AddServerDialog::~AddServerDialog()
{
	delete d;
}

void AddServerDialog::setListServerModel(sessionlisting::ListServerModel *model)
{
	d->listServerModel = model;
}

void AddServerDialog::query(const QUrl &url)
{
	d->ui.urlEdit->setText(url.toString());
	fetchApiInfo();
}

void AddServerDialog::updateCheckButton()
{
	int page = d->ui.pages->currentIndex();
	if(page == URL_PAGE) {
		QUrl url = getUrlFromInput();
		d->okButton->setEnabled(
			url.isValid() && !url.host().isEmpty() && !d->apiResponse);
	}
}

void AddServerDialog::handleButtonClick(QAbstractButton *button)
{
	int page = d->ui.pages->currentIndex();
	if(page == URL_PAGE) {
		if(button == d->okButton) {
			fetchApiInfo();
		} else if(button == d->cancelButton) {
			reject();
		}
	} else if(page == PROGRESS_PAGE) {
		if(button == d->cancelButton) {
			d->cancelRequested = true;
			d->cancelButton->setEnabled(false);
			if(d->apiResponse) {
				d->apiResponse->cancel();
			}
			if(d->faviconDownload) {
				d->faviconDownload->cancel();
			}
		}
	} else if(page == RESULT_PAGE) {
		if(button == d->okButton) {
			addServer();
		} else if(button == d->cancelButton) {
			setPage(URL_PAGE);
		}
	} else {
		qWarning("Unknown page %d", page);
		return;
	}
}

void AddServerDialog::handleCheckFinished(
	const QVariant &result, const QString &message, const QString &error)
{
	Q_UNUSED(message);
	sessionlisting::AnnouncementApiResponse *apiResponse = d->apiResponse;
	d->apiResponse = nullptr;
	if(d->cancelRequested) {
		setPage(URL_PAGE);
	} else if(error.isEmpty()) {
		d->serverInfo = result.value<sessionlisting::ListServerInfo>();
		d->url = apiResponse->apiUrl();
		fetchFavicon();
	} else if(!retryFetchApiInfoWithHttp(apiResponse)) {
		d->ui.errorLabel->setText(
			tr("<strong>Error:</strong> %1").arg(error.toHtmlEscaped()));
		d->ui.errorLabel->show();
		setPage(URL_PAGE);
	}
	apiResponse->deleteLater();
}

void AddServerDialog::updateFaviconDownloadProgress(
	qint64 bytesReceived, qint64 bytesTotal)
{
	if(bytesTotal <= 0) {
		d->ui.progressBar->setRange(0, 0);
	} else {
		d->ui.progressBar->setRange(0, 100);
		d->ui.progressBar->setValue(
			qRound(double(bytesTotal) / double(bytesReceived) * 100.0));
	}
}

void AddServerDialog::handleFaviconDownloadFinished(const QString &error)
{
	QImage favicon;
	if(d->cancelRequested) {
		setPage(URL_PAGE);
		return;
	} else if(!error.isEmpty()) {
		qWarning("Couldn't fetch favicon: %s", qUtf8Printable(error));
	} else if(!favicon.load(d->faviconDownload->file(), nullptr)) {
		qWarning("Couldn't load favicon");
	} else if(
		favicon.width() > FAVICON_SIZE || favicon.height() > FAVICON_SIZE) {
		favicon = favicon.scaled(
			FAVICON_SIZE, FAVICON_SIZE, Qt::KeepAspectRatio,
			Qt::SmoothTransformation);
	}
	d->faviconDownload->deleteLater();
	d->faviconDownload = nullptr;
	handleFaviconReceived(favicon.isNull() ? getFallbackFavicon() : favicon);
}

QUrl AddServerDialog::getUrlFromInput() const
{
	QString urlString = d->ui.urlEdit->text().trimmed();
	QUrl url = QUrl::fromUserInput(urlString);
	if(url.isValid() && url.scheme().startsWith("http", Qt::CaseInsensitive)) {
		// Qt will guess http by default, but we'd rather use https. If this
		// doesn't work because of certificate issues, we fall back later.
		if(!urlString.startsWith("http://", Qt::CaseInsensitive)) {
			url.setScheme("https");
		}
	}
	return url;
}

void AddServerDialog::fetchApiInfo()
{
	QUrl url = getUrlFromInput();
	if(!d->apiResponse && !d->faviconDownload) {
		d->ui.errorLabel->hide();
		d->ui.progressBar->setRange(0, 0);
		setPage(PROGRESS_PAGE);
		d->httpFallbackAttempted = false;
		initiateFetchApiInfo(url);
	}
}

bool AddServerDialog::retryFetchApiInfoWithHttp(
	const sessionlisting::AnnouncementApiResponse *apiResponse)
{
	QUrl url = d->requestUrl;
	bool shouldRetryWithoutHttps =
		!d->cancelRequested && !d->httpFallbackAttempted &&
		url.scheme().startsWith("https", Qt::CaseInsensitive) &&
		apiResponse->networkError() == QNetworkReply::SslHandshakeFailedError;
	if(shouldRetryWithoutHttps) {
		d->httpFallbackAttempted = true;
		url.setScheme("http");
		initiateFetchApiInfo(url);
		return true;
	} else {
		return false;
	}
}

void AddServerDialog::initiateFetchApiInfo(const QUrl &url)
{
	updateRequestUrl(url);
	d->cancelRequested = false;
	d->apiResponse = sessionlisting::getApiInfo(url);
	connect(
		d->apiResponse,
		&sessionlisting::AnnouncementApiResponse::requestUrlChanged, this,
		&AddServerDialog::updateRequestUrl);
	connect(
		d->apiResponse, &sessionlisting::AnnouncementApiResponse::finished,
		this, &AddServerDialog::handleCheckFinished);
}

void AddServerDialog::fetchFavicon()
{
	if(d->serverInfo.faviconUrl == "drawpile") {
		handleFaviconReceived(getFallbackFavicon());
	} else {
		const QUrl url(d->serverInfo.faviconUrl);
		if(url.isValid()) {
			d->ui.progressLabel->setText(
				tr("Retrieving %1...").arg(url.toString()));
			d->faviconDownload = new networkaccess::FileDownload(this);
			d->faviconDownload->setExpectedType("image/");
			connect(
				d->faviconDownload, &networkaccess::FileDownload::finished,
				this, &AddServerDialog::handleFaviconDownloadFinished);
			connect(
				d->faviconDownload, &networkaccess::FileDownload::progress,
				this, &AddServerDialog::updateFaviconDownloadProgress);
			d->faviconDownload->start(url);
		} else {
			handleFaviconReceived(getFallbackFavicon());
		}
	}
}

void AddServerDialog::handleFaviconReceived(const QImage &favicon)
{
	d->favicon = favicon;
	QString resultText =
		QStringLiteral(
			"<table>"
			"    <tr>"
			"	     <td rowspan=\"2\" width=\"80\" align=\"center\" "
			"			     valign=\"middle\">"
			"		     <img src=\"favicon\" width=\"64\" height=\"64\">"
			"		 </td>"
			"		 <td><h1 style=\"pre-wrap\">%1</h1></td>"
			"    </tr>"
			"	 <tr>"
			"	     <td><code style=\"pre-wrap\">%2</code></td>"
			"	 </tr>"
			"     <tr>"
			"	     <td colspan=\"2\" style=\"pre-wrap\"><hr>%3</td>"
			"     </tr>"
			"</table>")
			.arg(
				d->serverInfo.name.toHtmlEscaped(),
				d->url.toString().toHtmlEscaped(),
				d->serverInfo.description.toHtmlEscaped());
	d->browser->setText(resultText);
	setPage(RESULT_PAGE);
}

QImage AddServerDialog::getFallbackFavicon()
{
	return QIcon{":/icons/drawpile.png"}.pixmap(FAVICON_SIZE).toImage();
}

void AddServerDialog::addServer()
{
	sessionlisting::ListServerModel *listServerModel =
		d->listServerModel ? d->listServerModel
						   : new sessionlisting::ListServerModel(
								 dpApp().settings(), true, this);

	QString urlString = d->url.toString();
	listServerModel->addServer(
		d->serverInfo.name, urlString, d->serverInfo.description,
		d->serverInfo.readOnly, d->serverInfo.publicListings);
	QIcon icon = listServerModel->setFavicon(urlString, d->favicon);

	listServerModel->submit();
	emit serverAdded(d->serverInfo.name, icon);
	accept();
}

void AddServerDialog::setPage(int page)
{
	d->cancelButton->setEnabled(true);
	if(page == URL_PAGE) {
		d->ui.urlEdit->setFocus();
		d->okButton->show();
		d->okButton->setText(tr("Check"));
		updateCheckButton();
	} else if(page == PROGRESS_PAGE) {
		d->okButton->hide();
	} else if(page == RESULT_PAGE) {
		d->okButton->show();
		d->okButton->setEnabled(true);
		d->okButton->setText(tr("Add"));
	} else {
		qWarning("Unknown page %d", page);
		return;
	}
	d->ui.pages->setCurrentIndex(page);
}

void AddServerDialog::updateRequestUrl(const QUrl &url)
{
	d->requestUrl = url;
	d->ui.progressLabel->setText(tr("Checking %1...").arg(url.toString()));
}
}
