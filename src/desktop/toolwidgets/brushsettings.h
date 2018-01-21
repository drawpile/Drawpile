/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2017 Calle Laakkonen

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
#ifndef TOOLSETTINGS_BRUSHES_H
#define TOOLSETTINGS_BRUSHES_H

#include "toolsettings.h"

#include <QWidget>
#include <QAbstractListModel>

class QAction;

namespace tools {

/**
 * @brief Brush settings
 *
 * This is a settings class for the brush tool.
 */
class BrushSettings : public ToolSettings {
	Q_OBJECT
	friend class AdvancedBrushSettings;
public:
	BrushSettings(ToolController *ctrl, QObject *parent=nullptr);
	~BrushSettings();

	QString toolType() const override { return QStringLiteral("brush"); }

	void setActiveTool(tools::Tool::Type tool) override;
	void setForeground(const QColor& color) override;
	void quickAdjust1(float adjustment) override;

	int getSize() const override;
	bool getSubpixelMode() const override;

	void pushSettings() override;
	ToolProperties saveToolSettings() override;
	void restoreToolSettings(const ToolProperties &cfg) override;

	void setCurrentBrushSettings(const ToolProperties &brushProps);
	ToolProperties getCurrentBrushSettings() const;

	int currentBrushSlot() const;
	bool isCurrentEraserSlot() const;

public slots:
	void selectBrushSlot(int i);
	void selectEraserSlot(bool eraser);
	void toggleEraserMode() override;

signals:
	void colorChanged(const QColor &color);
	void eraseModeChanged(bool erase);
	void subpixelModeChanged(bool subpixel);

protected:
	QWidget *createUiWidget(QWidget *parent) override;

private slots:
	void selectBlendMode(int);
	void setEraserMode(bool erase);
	void updateUi();
	void updateFromUi();

private:
	struct Private;
	Private *d;
};

class BrushPresetModel : public QAbstractListModel {
	Q_OBJECT
public:
	enum BrushPresetRoles {
		ToolPropertiesRole = Qt::UserRole + 1
	};

	static BrushPresetModel *getSharedInstance();

	explicit BrushPresetModel(QObject *parent=nullptr);
	~BrushPresetModel();

	int rowCount(const QModelIndex &parent=QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	QMap<int,QVariant> itemData(const QModelIndex &index) const override;
	bool setData(const QModelIndex &index, const QVariant &value, int role=Qt::EditRole) override;
	bool setItemData(const QModelIndex &index, const QMap<int,QVariant> &roles) override;
	bool insertRows(int row, int count, const QModelIndex &parent=QModelIndex()) override;
	bool removeRows(int row, int count, const QModelIndex &parent=QModelIndex()) override;
	Qt::DropActions supportedDropActions() const override;
	void addBrush(const ToolProperties &brushProps);

	void saveBrushes() const;
	void loadBrushes();
	void makeDefaultBrushes();

private:
	struct Private;
	Private *d;
};

}

#endif

