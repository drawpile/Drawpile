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

#include "desktop/scene/canvasviewmodifiers.h"

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

CanvasViewShortcuts CanvasViewShortcuts::load(const QSettings &cfg)
{
	CanvasViewShortcuts s;
	read(cfg, s.colorPick, QStringLiteral("colorPick"));
	read(cfg, s.layerPick, QStringLiteral("layerPick"));
	read(cfg, s.dragRotate, QStringLiteral("dragRotate"));
	read(cfg, s.dragZoom, QStringLiteral("dragZoom"));
	read(cfg, s.dragQuickAdjust, QStringLiteral("dragQuickAdjust"));
	read(cfg, s.scrollRotate, QStringLiteral("scrollRotate"));
	read(cfg, s.scrollZoom, QStringLiteral("scrollZoom"));
	read(cfg, s.scrollQuickAdjust, QStringLiteral("scrollQuickAdjust"));
	read(cfg, s.toolConstraint1, QStringLiteral("toolConstraint1"));
	read(cfg, s.toolConstraint2, QStringLiteral("toolConstraint2"));

	return s;
}

void CanvasViewShortcuts::save(QSettings &cfg) const
{
	const CanvasViewShortcuts defaults;

	write(cfg, colorPick, defaults.colorPick, QStringLiteral("colorPick"));
	write(cfg, layerPick, defaults.layerPick, QStringLiteral("layerPick"));
	write(cfg, dragRotate, defaults.dragRotate, QStringLiteral("dragRotate"));
	write(cfg, dragZoom, defaults.dragZoom, QStringLiteral("dragZoom"));
	write(cfg, dragQuickAdjust, defaults.dragQuickAdjust, QStringLiteral("dragQuickAdjust"));
	write(cfg, scrollRotate, defaults.scrollRotate, QStringLiteral("scrollRotate"));
	write(cfg, scrollZoom, defaults.scrollZoom, QStringLiteral("scrollZoom"));
	write(cfg, scrollQuickAdjust, defaults.scrollQuickAdjust, QStringLiteral("scrollQuickAdjust"));
	write(cfg, toolConstraint1, defaults.toolConstraint1, QStringLiteral("toolConstraint1"));
	write(cfg, toolConstraint2, defaults.toolConstraint2, QStringLiteral("toolConstraint2"));
}
