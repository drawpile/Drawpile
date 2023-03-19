/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 - 2022 Calle Laakkonen, askmeaboutloom

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

#ifndef DP_BRUSHPRESETMODEL_H
#define DP_BRUSHPRESETMODEL_H

#include <QAbstractItemModel>

class QFile;
class QFileInfo;

namespace drawdance {
	class ZipReader;
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

class BrushPresetModel;

class BrushPresetTagModel : public QAbstractItemModel {
	Q_OBJECT
	friend class BrushPresetModel;
public:
	enum Roles {
		SortRole = Qt::UserRole + 1,
	};

	explicit BrushPresetTagModel(QObject *parent = nullptr);
	virtual ~BrushPresetTagModel();

	BrushPresetModel *presetModel() { return m_presetModel; }

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	QModelIndex parent(const QModelIndex &index) const override;
	QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const override;

	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

	Tag getTagAt(int row) const;
	int getTagRowById(int tagId) const;

	int newTag(const QString& name);
	int editTag(int tagId, const QString &name);
	void deleteTag(int tagId);

	void setState(const QString &key, const QVariant &value);
	QVariant getState(const QString &key) const;

	bool importMyPaintBrushPack(
		const QString &file, int &outTagId, QString &outTagName,
		QStringList &outErrors);

private:
	class Private;
	Private *d;
	BrushPresetModel *m_presetModel;

	static bool isBuiltInTag(int row);

	void maybeConvertOldPresets();
	void convertOldPresets();

	static bool readMyPaintBrush(
		const drawdance::ZipReader &zr, const QString &prefix, QStringList &outErrors,
		ActiveBrush &outBrush, QString &outDescription, QPixmap &outThumbnail);
};

class BrushPresetModel : public QAbstractItemModel {
	Q_OBJECT
	friend class BrushPresetTagModel;

public:
	enum Roles {
		SortRole = Qt::UserRole + 1,
		FilterRole,
		BrushRole,
	};

	explicit BrushPresetModel(BrushPresetTagModel *tagModel);
	virtual ~BrushPresetModel();

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	QModelIndex parent(const QModelIndex &index) const override;
	QModelIndex index(int row, int column = 0, const QModelIndex &parent = QModelIndex()) const override;

	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

	int getIdFromIndex(const QModelIndex &index) { return index.isValid() ? index.internalId() : 0; };

	void setTagIdToFilter(int tagId);

	QList<TagAssignment> getTagAssignments(int presetId);
	bool changeTagAssignment(int presetId, int tagId, bool assigned);

	PresetMetadata getPresetMetadata(int presetId);

	int newPreset(const QString &type, const QString &name, const QString description,
		const QPixmap &thumbnail, const QByteArray &data);
	int duplicatePreset(int presetId);
	bool updatePresetData(int presetId, const QString &type, const QByteArray &data);
	bool updatePresetMetadata(int presetId, const QString &name, const QString &description,
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
