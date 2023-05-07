// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBCLIENT_UTILS_CERTIFICATESTOREMODEL_H
#define LIBCLIENT_UTILS_CERTIFICATESTOREMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QModelIndex>
#include <QSslCertificate>
#include <QString>
#include <functional>
#include <optional>
#include <utility>

namespace libclient { namespace settings { class Settings; } }

class CertificateStoreModel final : public QAbstractListModel
{
	Q_OBJECT
public:
	enum {
		TrustedRole = Qt::UserRole
	};

	explicit CertificateStoreModel(QObject *parent = nullptr);
	/// @brief Add a new certificate to the store from the given path
	std::pair<QModelIndex, QString> addCertificate(const QString &path, bool trusted);
	// Not able to use `QAbstractListModel::data` because it is not possible to
	// return a reference to `QSslCertificate` in `QVariant`
	std::optional<std::reference_wrapper<const QSslCertificate>> certificate(const QModelIndex &index) const;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	QString lastError() const { return m_lastError; }
	bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	bool setData(const QModelIndex &index, const QVariant &value, int role) override;

public slots:
	void revert() override;
	bool submit() override;

private:
	enum class Pending {
		None,
		Copy,
		Move,
		Delete
	};

	struct Certificate {
		QString path;
		mutable std::optional<QSslCertificate> certificate;
		bool trusted;
		Pending pending;
	};

	void loadCertificatesFromPath(const QString &path, bool trusted);
	const std::optional<QSslCertificate> &readCertificate(const Certificate &cert) const;
	void reloadCertificatesFromDisk();

	QList<Certificate> m_certificates;
	QString m_lastError;
};

#endif
