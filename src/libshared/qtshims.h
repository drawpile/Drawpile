/*
 * Drawpile - a collaborative drawing program.
 *
 * Copyright (C) 2023 askmeaboutloom
 *
 * Drawpile is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Drawpile is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef QTSHIMS_H
#define QTSHIMS_H
#include <QLibraryInfo>
#include <QKeyEvent>
#include <QtGlobal>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)

namespace shim {

using EnterEvent = QEvent;

constexpr auto ERASER_TYPE = QTabletEvent::Eraser;

inline int getKey(const QKeyEvent *e)
{
	return e->key();
}

inline QString translationsPath()
{
    return QLibraryInfo::location(QLibraryInfo::TranslationsPath);
}

}

#else

namespace shim {

using EnterEvent = QEnterEvent;

constexpr auto ERASER_TYPE = QPointingDevice::PointerType::Eraser;

inline QKeyCombination getKey(const QKeyEvent *e)
{
	return e->keyCombination();
}

inline QString translationsPath()
{
    return QLibraryInfo::path(QLibraryInfo::TranslationsPath);
}

}

#endif

#endif
