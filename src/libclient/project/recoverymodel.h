// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_PROJECT_RECOVERYMODEL_H
#define LIBCLIENT_PROJECT_RECOVERYMODEL_H
#include <QAbstractItemModel>
#include <QDateTime>
#include <QFileInfo>
#include <QPixmap>
#include <QSet>
#include <QString>

struct DP_ProjectRecoveryInfo;

namespace project {

enum class RecoveryStatus {
	Available,
	Locked,
	Error,
};

class RecoveryEntry final {
public:
	explicit RecoveryEntry(const QFileInfo &fileInfo);

	const QString &path() const { return m_path; }
	const QString &baseName() const { return m_baseName; }

	RecoveryStatus status() const;
	qint64 fileSize() const;
	const QDateTime &mtime() const;
	const QPixmap &thumbnail() const;
	const QString &lastSave() const;
	const QString &lastExport() const;
	const QString &lastSessionTitle() const;
	long long ownWorkMinutes() const;
	const QString &errorMessage() const { return m_errorMessage; }

	// Returns the first non-empty last save base name, last export base name or
	// the base name of the recovery file itself.
	QString mostDescriptiveBaseName() const;

private:
	void loadMetadata() const;

	static void stripExtension(QString &s);

	QFileInfo m_fileInfo;
	QString m_path;
	QString m_baseName;
	mutable RecoveryStatus m_status;
	mutable qint64 m_fileSize;
	mutable QDateTime m_mtime;
	mutable QString m_errorMessage;
	mutable QPixmap m_thumbnail;
	mutable QString m_lastSave;
	mutable QString m_lastExport;
	mutable QString m_lastSessionTitle;
	mutable long long m_ownWorkMinutes = -1LL;
	mutable bool m_statusLoaded = false;
	mutable bool m_thumbnailLoaded = false;
	mutable bool m_fileSizeLoaded = false;
	mutable bool m_mtimeLoaded = false;
	mutable bool m_metadataLoaded = false;
};

class RecoveryModel final : public QAbstractItemModel {
	Q_OBJECT
public:
	explicit RecoveryModel(const QString &baseDir, QObject *parent = nullptr);

	QModelIndex index(
		int row, int column,
		const QModelIndex &parent = QModelIndex()) const override;

	QModelIndex parent(const QModelIndex &child) const override;

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;

	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

	bool checkPotentialEntries();

	void load();
	void reload();

	const QVector<RecoveryEntry> entries() const { return m_entries; }

	void removeOrphanedFiles();

	static void addOpenedAutosavePath(const QString &path);
	static void removeOpenedAutosavePath(const QString &path);

	static QFileInfoList entryInfoListForBaseDir(const QString &baseDir);

	static RecoveryStatus checkRecoveryStatus(
		const QString &path, QString *outErrorMessage = nullptr);

Q_SIGNALS:
	void loaded();

private:
	bool isValidIndex(int row, int column, const QModelIndex &parent) const
	{
		return !parent.isValid() && column == 0 && row >= 0 &&
			   row < m_entries.size();
	}

	QFileInfoList entryInfoList() const;

	static bool entryLessThan(const RecoveryEntry &a, const RecoveryEntry &b);

	QString m_baseDir;
	QVector<RecoveryEntry> m_entries;
	bool m_loaded = false;

	// Autosave paths that are open in our own process. Open paths in other
	// processes will show up in state Locked instead.
	static QSet<QString> openedAutosavePaths;
};

}

#endif
