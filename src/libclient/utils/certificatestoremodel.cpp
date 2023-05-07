// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/certificatestoremodel.h"
#include "libshared/util/paths.h"

#include <QDir>
#include <QFile>
#include <QIcon>
#include <QSslCertificate>
#include <QUrl>

CertificateStoreModel::CertificateStoreModel(QObject *parent)
	: QAbstractListModel(parent)
{
	reloadCertificatesFromDisk();
}

std::pair<QModelIndex, QString> CertificateStoreModel::addCertificate(const QString &path, bool trusted)
{
	const auto certs = QSslCertificate::fromPath(path);
	if (certs.isEmpty()) {
		return {
			{},
			tr("'%1' does not contain any valid host certificates.").arg(path)
		};
	}

	if (certs.size() != 1) {
		return {
			{},
			tr("'%1' contains multiple host certificates, "
				"which is not currently supported.").arg(path)
		};
	}

	const auto &cert = certs.at(0);

	if (cert.isNull() || cert.subjectInfo(QSslCertificate::CommonName).isEmpty()) {
		return {
			{},
			tr("'%1' contains an invalid certificate.").arg(path)
		};
	}

	const auto hostnames = cert.subjectInfo(QSslCertificate::CommonName);
	if (hostnames.size() != 1) {
		return {
			{},
			tr("'%1' contains a certificate with multiple hostnames, "
				"which is not currently supported.").arg(path)
		};
	}

	const auto row = m_certificates.size();

	beginInsertRows(QModelIndex(), row, row);

	m_certificates.append({
		path,
		std::move(cert),
		trusted,
		Pending::Copy
	});

	endInsertRows();

	return { createIndex(row, 0), {} };
}

std::optional<std::reference_wrapper<const QSslCertificate>> CertificateStoreModel::certificate(const QModelIndex &index) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_certificates.size()) {
		return std::nullopt;
	}

	return readCertificate(m_certificates.at(index.row()));
}

QVariant CertificateStoreModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_certificates.size()) {
		return QVariant();
	}

	const auto &item = m_certificates.at(index.row());

	switch (role) {
	case TrustedRole:
		return item.trusted;
	case Qt::DecorationRole:
		if (item.trusted) {
			return QIcon::fromTheme("security-high");
		} else {
			QPixmap pm(64, 64);
			pm.fill(Qt::transparent);
			return QIcon(pm);
		}
		break;
	case Qt::DisplayRole:
		// QSslCertificate has a slow one-time startup initialisation on at
		// least macOS, so only use the CN from the certificate if it is already
		// loaded.
		if (const auto &certificate = item.certificate) {
			return certificate->subjectInfo(QSslCertificate::CommonName).at(0);
		} else {
			auto filename = QUrl::fromLocalFile(item.path).fileName();
			return filename.left(filename.size() - 4);
		}
		break;
	}

	return QVariant();
}

bool CertificateStoreModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if (row < 0 || row + count > m_certificates.size()) {
		return false;
	}

	beginRemoveRows(parent, row, row + count - 1);

	for (auto i = row; i < row + count; ++i) {
		auto &item = m_certificates[i];
		if (item.pending == Pending::Copy) {
			item.pending = Pending::None;
		} else {
			item.pending = Pending::Delete;
		}
	}

	endRemoveRows();

	return true;
}

int CertificateStoreModel::rowCount(const QModelIndex &) const
{
	return m_certificates.size();
}

bool CertificateStoreModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_certificates.size()) {
		return false;
	}

	auto &item = m_certificates[index.row()];

	if (item.pending == Pending::Delete) {
		return false;
	}

	switch (role) {
	case TrustedRole: {
		const auto trusted = value.toBool();
		if (trusted != item.trusted) {
			item.trusted = value.toBool();
			if (item.pending != Pending::Copy) {
				item.pending = Pending::Move;
			}
			emit dataChanged(index, index, { role, Qt::DecorationRole });
		}
		return true;
	}
	}

	return false;
}

void CertificateStoreModel::revert()
{
	reloadCertificatesFromDisk();
}

bool CertificateStoreModel::submit()
{
	for (auto index = m_certificates.size() - 1; index >= 0; --index) {
		auto &cert = m_certificates[index];

		switch (cert.pending) {
		case Pending::None:
			break;
		case Pending::Copy:
		case Pending::Move: {
			const auto &info = readCertificate(cert);
			if (!info) {
				m_lastError = tr("Could not read certificate from '%1'.").arg(cert.path);
				return false;
			}

			auto to = utils::paths::writablePath(
				cert.trusted ? "trusted-hosts" : "known-hosts",
				info->subjectInfo(QSslCertificate::CommonName).at(0) + ".pem"
			);

			if (cert.pending == Pending::Copy) {
				auto file = QFile(to);
				if (!file.open(QFile::WriteOnly)) {
					m_lastError = tr("Could not open '%1' for writing: %2.")
						.arg(file.fileName())
						.arg(file.errorString());
					return false;
				}

				const auto data = info->toPem();
				if (file.write(data) != data.size()) {
					m_lastError = tr("Could not write '%1': %2.")
						.arg(file.fileName())
						.arg(file.errorString());
				}
			} else {
				auto file = QFile(cert.path);
				if (!file.rename(to)) {
					m_lastError = tr("Could not move '%1' to '%2': %3.")
						.arg(file.fileName())
						.arg(to)
						.arg(file.errorString());
					return false;
				}
			}

			cert.path = to;
			cert.pending = Pending::None;

			break;
		}
		case Pending::Delete: {
			auto file = QFile(cert.path);
			if (!file.remove()) {
				m_lastError = tr("Could not delete '%1': %2.")
					.arg(file.fileName())
					.arg(file.errorString());
				return false;
			}

			m_certificates.removeAt(index);
			break;
		}
		}
	}

	m_lastError.clear();
	return true;
}

void CertificateStoreModel::loadCertificatesFromPath(const QString &path, bool trusted)
{
	QDir dir(utils::paths::writablePath(path));

	for (const auto &filename : dir.entryList({ "*.pem" }, QDir::Files)) {
		const auto host = filename.left(filename.size() - 4);
		m_certificates.append({
			dir.absoluteFilePath(filename),
			{},
			trusted,
			Pending::None
		});
	}
}

const std::optional<QSslCertificate> &CertificateStoreModel::readCertificate(const Certificate &cert) const
{
	if (!cert.certificate) {
		const auto certs = QSslCertificate::fromPath(cert.path);
		if (certs.size() == 1) {
			cert.certificate.emplace(std::move(certs.at(0)));
		}
	}

	return cert.certificate;
}

void CertificateStoreModel::reloadCertificatesFromDisk()
{
	m_certificates.clear();
	loadCertificatesFromPath("known-hosts", false);
	loadCertificatesFromPath("trusted-hosts", true);
}
