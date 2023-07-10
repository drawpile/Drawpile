// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ADDSERVERDIALOG_H
#define ADDSERVERDIALOG_H

#include "libclient/net/sessionlistingmodel.h"

#include <QDialog>

class QAbstractButton;
class QImage;

namespace sessionlisting {
class AnnouncementApiResponse;
class ListServerModel;
}

namespace dialogs {

class AddServerDialog final : public QDialog {
	Q_OBJECT
public:
	explicit AddServerDialog(QWidget *parent = nullptr);
	~AddServerDialog() override;

	void query(const QUrl &query);
	void setListServerModel(sessionlisting::ListServerModel *model);

signals:
	void serverAdded(const QString &name, const QIcon &icon);

private slots:
	void updateCheckButton();
	void handleButtonClick(QAbstractButton *button);

	void handleCheckFinished(
		const QVariant &result, const QString &message, const QString &error);

	void updateFaviconDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
	void handleFaviconDownloadFinished(const QString &error);

private:
	static constexpr int URL_PAGE = 0;
	static constexpr int PROGRESS_PAGE = 1;
	static constexpr int RESULT_PAGE = 2;
	static constexpr int FAVICON_SIZE = 128;

	QUrl getUrlFromInput() const;

	void fetchApiInfo();
	bool retryFetchApiInfoWithHttp(
		const sessionlisting::AnnouncementApiResponse *apiResponse);
	void initiateFetchApiInfo(const QUrl &url);

	void fetchFavicon();
	void handleFaviconReceived(const QImage &favicon);
	static QImage getFallbackFavicon();

	void addServer();

	void setPage(int page);
	void updateRequestUrl(const QUrl &url);

	struct Private;
	Private *d;
};

}

#endif
