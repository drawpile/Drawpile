/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#include "canvasviewmodifiers.h"

#include <QSettings>

static void read(const QSettings &cfg, CanvasViewShortcut &shortcut, const QString &name)
{
	const QVariant v = cfg.value(name);
	if(v.isValid()) {
		const auto flags = v.toString().split('+');
		shortcut.modifiers.setFlag(Qt::ShiftModifier, flags.contains(QStringLiteral("shift")));
		shortcut.modifiers.setFlag(Qt::ControlModifier, flags.contains(QStringLiteral("ctrl")));
		shortcut.modifiers.setFlag(Qt::AltModifier, flags.contains(QStringLiteral("alt")));
		shortcut.modifiers.setFlag(Qt::MetaModifier, flags.contains(QStringLiteral("meta")));
	}
}

static void write(QSettings &cfg, const CanvasViewShortcut &shortcut, const CanvasViewShortcut &defaultShortcut, const QString &name)
{
	if(shortcut.modifiers == defaultShortcut.modifiers) {
		cfg.remove(name);

	} else {
		QStringList flags;
		if(shortcut.modifiers.testFlag(Qt::ShiftModifier)) flags << QStringLiteral("shift");
		if(shortcut.modifiers.testFlag(Qt::ControlModifier)) flags << QStringLiteral("ctrl");
		if(shortcut.modifiers.testFlag(Qt::AltModifier)) flags << QStringLiteral("alt");
		if(shortcut.modifiers.testFlag(Qt::MetaModifier)) flags << QStringLiteral("meta");
		cfg.setValue(name, flags.join('+'));
	}
}

void CanvasViewShortcuts::load(const QSettings &cfg)
{
	read(cfg, colorPick, QStringLiteral("colorPick"));
	read(cfg, layerPick, QStringLiteral("layerPick"));
	read(cfg, dragRotate, QStringLiteral("dragRotate"));
	read(cfg, dragZoom, QStringLiteral("dragZoom"));
	read(cfg, dragQuickAdjust, QStringLiteral("dragQuickAdjust"));
	read(cfg, scrollZoom, QStringLiteral("scrollZoom"));
	read(cfg, scrollQuickAdjust, QStringLiteral("scrollQuickAdjust"));
	read(cfg, toolConstraint1, QStringLiteral("toolConstraint1"));
	read(cfg, toolConstraint2, QStringLiteral("toolConstraint2"));
}

void CanvasViewShortcuts::save(QSettings &cfg) const
{
	const CanvasViewShortcuts defaults;

	write(cfg, colorPick, defaults.colorPick, QStringLiteral("colorPick"));
	write(cfg, layerPick, defaults.layerPick, QStringLiteral("layerPick"));
	write(cfg, dragRotate, defaults.dragRotate, QStringLiteral("dragRotate"));
	write(cfg, dragZoom, defaults.dragZoom, QStringLiteral("dragZoom"));
	write(cfg, dragQuickAdjust, defaults.dragQuickAdjust, QStringLiteral("dragQuickAdjust"));
	write(cfg, scrollZoom, defaults.scrollZoom, QStringLiteral("scrollZoom"));
	write(cfg, scrollQuickAdjust, defaults.scrollQuickAdjust, QStringLiteral("scrollQuickAdjust"));
	write(cfg, toolConstraint1, defaults.toolConstraint1, QStringLiteral("toolConstraint1"));
	write(cfg, toolConstraint2, defaults.toolConstraint2, QStringLiteral("toolConstraint2"));
}
