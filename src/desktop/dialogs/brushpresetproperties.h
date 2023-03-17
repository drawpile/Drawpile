/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2022 askmeaboutloom

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

#ifndef BRUSHPRESETPROPERTIES_H
#define BRUSHPRESETPROPERTIES_H

#include <QDialog>

class Ui_BrushPresetProperties;

namespace dialogs {

class BrushPresetProperties final : public QDialog
{
Q_OBJECT
public:
	explicit BrushPresetProperties(int id, const QString &name, const QString &description,
        const QPixmap &thumbnail, QWidget *parent = nullptr);

	~BrushPresetProperties() override;

signals:
    void presetPropertiesApplied(int id, const QString &name, const QString &description,
        const QPixmap &thumbnail);

private slots:
    void chooseThumbnailFile();
    void emitChanges();

private:
    int m_id;
    QPixmap m_thumbnail;
    Ui_BrushPresetProperties *m_ui;

    void showThumbnail(const QPixmap &thumbnail);
};

}

#endif
