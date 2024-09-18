// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_BRUSHES_BRUSHPRESETMODEL_H
#define LIBCLIENT_BRUSHES_BRUSHPRESETMODEL_H
#include "libclient/brushes/brush.h"
#include <QAbstractItemModel>
#include <QJsonValue>
#include <QPixmap>
#include <optional>

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

	bool isAssignable() const { return id > 0; }
	bool isEditable() const { return id > 0; }
};

struct TagAssignment {
	int id;
	QString name;
	bool assigned;
};

struct Preset {
	int id = 0;
	QString originalName;
	QString originalDescription;
	QPixmap originalThumbnail;
	ActiveBrush originalBrush;
	std::optional<QString> changedName;
	std::optional<QString> changedDescription;
	std::optional<QPixmap> changedThumbnail;
	std::optional<ActiveBrush> changedBrush;

	const QString &effectiveName() const
	{
		return changedName.has_value() ? changedName.value() : originalName;
	}

	const QString &effectiveDescription() const
	{
		return changedDescription.has_value() ? changedDescription.value()
											  : originalDescription;
	}

	const QPixmap &effectiveThumbnail() const
	{
		return changedThumbnail.has_value() ? changedThumbnail.value()
											: originalThumbnail;
	}

	const ActiveBrush &effectiveBrush() const
	{
		return changedBrush.has_value() ? changedBrush.value() : originalBrush;
	}

	bool hasChanges() const
	{
		return changedName.has_value() || changedDescription.has_value() ||
			   changedThumbnail.has_value() || changedBrush.has_value();
	}
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
	enum class ImportBrushType { Unknown, Json, Old };

	struct ImportBrushGroup {
		QString name;
		QStringList brushes;
	};

	struct ImportOldValue {
		QString cname;
		double baseValue;
		QVector<QPointF> inputPoints;
	};

	static bool isBuiltInTag(int row);
	bool isTagRowInBounds(int row) const;

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

	static ImportBrushType guessImportBrushType(const QByteArray &mybBytes);

	static QJsonValue readImportBrushJson(
		BrushImportResult &result, const QString &mybPath,
		const QByteArray &mybBytes);

	static QJsonValue readImportBrushOld(
		BrushImportResult &result, const QString &mybPath,
		const QByteArray &mybBytes);

	static double scaleOldColorHue(double y);
	static double scaleOldColorSaturationOrValue(double y);

	void exportTag(
		BrushExportResult &result, QStringList &order, drawdance::ZipWriter &zw,
		const BrushExportTag &tag) const;

	void exportPreset(
		BrushExportResult &result, QStringList &order, drawdance::ZipWriter &zw,
		const QString &tagPath, int presetId) const;

	static QString getExportName(int presetId, const QString presetName);

	static QString makeExportSafe(const QString &t);

	class Private;
	Private *d;
	BrushPresetModel *m_presetModel;
};

class BrushPresetModel final : public QAbstractItemModel {
	Q_OBJECT
	friend class BrushPresetTagModel;

public:
	enum Roles {
		FilterRole = Qt::UserRole + 1,
		IdRole,
		EffectiveTitleRole,
		EffectiveThumbnailRole,
		EffectiveDescriptionRole,
		HasChangesRole,
		PresetRole,
	};

	explicit BrushPresetModel(BrushPresetTagModel *tagModel);

	bool isPresetRowInBounds(int row) const;

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int rowCountForTagId(int tagId) const;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	QModelIndex parent(const QModelIndex &index) const override;
	QModelIndex index(
		int row, int column = 0,
		const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex cachedIndexForId(int presetId);
	QModelIndex indexForTagId(int tagId, int row) const;

	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

	int getIdFromIndex(const QModelIndex &index);

	void setTagIdToFilter(int tagId);

	QList<TagAssignment> getTagAssignments(int presetId);
	bool changeTagAssignment(int presetId, int tagId, bool assigned);

	std::optional<Preset> searchPresetBrushData(int presetId);

	std::optional<Preset> newPreset(
		const QString &name, const QString description,
		const QPixmap &thumbnail, const ActiveBrush &brush, int tagId);

	bool updatePreset(
		int presetId, const QString &name, const QString &description,
		const QPixmap &thumbnail, const ActiveBrush &brush);

	bool deletePreset(int presetId);

	void changePreset(
		int presetId, const std::optional<QString> &name = {},
		const std::optional<QString> &description = {},
		const std::optional<QPixmap> &thumbnail = {},
		const std::optional<ActiveBrush> &brush = {},
		bool inEraserSlot = false);
	void resetAllPresetChanges();
	void writePresetChanges();

	int countNames(const QString &name) const;

	QSize iconSize() const;
	int iconDimension() const;
	void setIconDimension(int dimension);

public slots:
	void tagsAboutToBeReset();
	void tagsReset();

signals:
	void presetChanged(
		int presetId, const QString &name, const QString &description,
		const QPixmap &thumbnail, const ActiveBrush &brush);
	void presetRemoved(int presetId);

private:
	static QPixmap loadBrushPreview(const QFileInfo &fileInfo);
	static QByteArray toPng(const QPixmap &pixmap);

	BrushPresetTagModel::Private *d;
};

}

Q_DECLARE_METATYPE(brushes::Preset)

#endif
