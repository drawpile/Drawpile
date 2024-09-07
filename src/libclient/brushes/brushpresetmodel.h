// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_BRUSHES_BRUSHPRESETMODEL_H
#define LIBCLIENT_BRUSHES_BRUSHPRESETMODEL_H
#include <QAbstractItemModel>

class QFile;
class QFileInfo;

namespace drawdance {
class ZipReader;
class ZipWriter;
}

namespace brushes {

class ActiveBrush;
class MyPaintBrush;
class ClassicBrush;

struct Tag {
	int id;
	QString name;
	bool editable;
};

struct TagAssignment {
	int id;
	QString name;
	bool assigned;
};

struct PresetMetadata {
	int id;
	QString name;
	QString description;
	QByteArray thumbnail;
};

struct BrushImportResult {
	QStringList errors;
	QVector<Tag> importedTags;
	int importedBrushCount;
};

struct BrushExportResult {
	bool ok;
	QStringList errors;
	int exportedBrushCount;
};

struct BrushExportTag {
	QString name;
	QVector<int> presetIds;
};

class BrushPresetModel;

class BrushPresetTagModel final : public QAbstractItemModel {
	Q_OBJECT
	friend class BrushPresetModel;

public:
	explicit BrushPresetTagModel(QObject *parent = nullptr);
	~BrushPresetTagModel() override;

	BrushPresetModel *presetModel() { return m_presetModel; }

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	QModelIndex parent(const QModelIndex &index) const override;
	QModelIndex index(
		int row, int column = 0,
		const QModelIndex &parent = QModelIndex()) const override;

	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

	static bool isExportableRow(int row);

	Tag getTagAt(int row) const;
	int getTagRowById(int tagId) const;

	int newTag(const QString &name);
	int editTag(int tagId, const QString &name);
	void deleteTag(int tagId);

	void setState(const QString &key, const QVariant &value);
	QVariant getState(const QString &key) const;

	BrushImportResult importBrushPack(const QString &file);

	BrushExportResult exportBrushPack(
		const QString &file, const QVector<BrushExportTag> &exportTags) const;

private:
	struct ImportBrushGroup {
		QString name;
		QStringList brushes;
	};

	class Private;
	Private *d;
	BrushPresetModel *m_presetModel;

	static bool isBuiltInTag(int row);

	void maybeConvertOldPresets();
	void convertOldPresets();

	static QVector<ImportBrushGroup> readOrderConf(
		BrushImportResult &result, const QString &file,
		const drawdance::ZipReader &zr);

	static int
	addOrderConfGroup(QVector<ImportBrushGroup> &groups, const QString &name);

	static void addOrderConfBrush(QStringList &brushes, const QString &brush);

	void readImportBrushes(
		BrushImportResult &result, const drawdance::ZipReader &zr,
		const QString &groupName, const QStringList &brushes);

	static bool readImportBrush(
		BrushImportResult &result, const drawdance::ZipReader &zr,
		const QString &prefix, ActiveBrush &outBrush, QString &outDescription,
		QPixmap &outThumbnail);

	void exportTag(
		BrushExportResult &result, QStringList &order, drawdance::ZipWriter &zw,
		const BrushExportTag &tag) const;

	void exportPreset(
		BrushExportResult &result, QStringList &order, drawdance::ZipWriter &zw,
		const QString &tagPath, int presetId) const;

	static QString getExportName(int presetId, const QString presetName);

	static QString makeExportSafe(const QString &t);
};

class BrushPresetModel final : public QAbstractItemModel {
	Q_OBJECT
	friend class BrushPresetTagModel;

public:
	enum Roles {
		FilterRole = Qt::UserRole + 1,
		BrushRole,
		TitleRole,
		IdRole,
		ThumbnailRole,
	};

	explicit BrushPresetModel(BrushPresetTagModel *tagModel);
	~BrushPresetModel() override;

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int rowCountForTagId(int tagId) const;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	QModelIndex parent(const QModelIndex &index) const override;
	QModelIndex index(
		int row, int column = 0,
		const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex indexForTagId(int tagId, int row) const;

	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

	int getIdFromIndex(const QModelIndex &index)
	{
		return index.isValid() ? index.internalId() : 0;
	}

	void setTagIdToFilter(int tagId);

	QList<TagAssignment> getTagAssignments(int presetId);
	bool changeTagAssignment(int presetId, int tagId, bool assigned);

	PresetMetadata getPresetMetadata(int presetId);

	int newPreset(
		const QString &type, const QString &name, const QString description,
		const QPixmap &thumbnail, const QByteArray &data);
	int duplicatePreset(int presetId);
	bool
	updatePresetData(int presetId, const QString &type, const QByteArray &data);
	bool updatePresetMetadata(
		int presetId, const QString &name, const QString &description,
		const QPixmap &thumbnail);
	bool deletePreset(int presetId);

	QSize iconSize() const;
	int iconDimension() const;
	void setIconDimension(int dimension);

public slots:
	void tagsAboutToBeReset();
	void tagsReset();

private:
	BrushPresetTagModel::Private *d;
	int m_tagIdToFilter;

	static QPixmap loadBrushPreview(const QFileInfo &fileInfo);
	static QByteArray toPng(const QPixmap &pixmap);
};

}

#endif
