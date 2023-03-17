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
#include <QString>
#include <QtGlobal>
#ifdef HAVE_QT_GUI
#	include <QEvent>
#	include <QKeyEvent>
#	include <QTabletEvent>
#endif

namespace shim {

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
template <typename T>
inline T cast(T value) {
	return value;
}
#else
template <typename T, typename U>
inline auto cast(U value) {
	return static_cast<T>(value);
}
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
constexpr QString::SplitBehavior SKIP_EMPTY_PARTS = QString::SkipEmptyParts;
#else
constexpr Qt::SplitBehavior SKIP_EMPTY_PARTS = Qt::SkipEmptyParts;
#endif

inline QString translationsPath()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	return QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#else
	return QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#endif
}

#ifdef HAVE_QT_GUI

#	if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using EnterEvent = QEvent;
#	else
using EnterEvent = QEnterEvent;
#	endif

#	if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
constexpr QTabletEvent::PointerType ERASER_TYPE = QTabletEvent::Eraser;
#	else
constexpr QPointingDevice::PointerType ERASER_TYPE =
	QPointingDevice::PointerType::Eraser;
#	endif

inline int getKey(const QKeyEvent *e)
{
#	if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	return e->key();
#	else
	return e->keyCombination();
#	endif
}

#endif

}

#endif
