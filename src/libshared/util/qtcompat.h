// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_LIBSHARED_UTIL_QTCOMPAT_H
#define DP_LIBSHARED_UTIL_QTCOMPAT_H

#include <QtGlobal>
#include <QAbstractSocket>
#include <QDebug>
#include <QLibraryInfo>
#include <QVariant>
#include <limits>

#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
#define HAVE_QT_COMPAT_PBKDF2
#define HAVE_QT_COMPAT_HASH_LENGTH
#endif

namespace compat {

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
const auto SkipEmptyParts = Qt::SkipEmptyParts;
#else
const auto SkipEmptyParts = QString::SkipEmptyParts;
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
const auto SocketError = &QAbstractSocket::errorOccurred;
#else
const auto SocketError = QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error);
#endif

// Since 5.15.2, the behavior of QStringView::mid() is compatible with
// QString::mid().
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 2)
using StringView = QStringView;
#else
using StringView = QString;
#endif

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#define HAVE_QT_COMPAT_QVARIANT_ENUM
using NativeEventResult = qintptr *;
using RetrieveDataMetaType = QMetaType;
using sizetype = qsizetype;

inline auto castSize(size_t size) {
	Q_ASSERT(size <= size_t(std::numeric_limits<qsizetype>().max()));
	return qsizetype(size);
}

inline auto isImageMime(const QString &mimeType, RetrieveDataMetaType) {
	return mimeType == "application/x-qt-image";
}

inline auto libraryPath(QLibraryInfo::LibraryPath p) {
	return QLibraryInfo::path(p);
}

inline auto metaTypeFromName(const char *name) {
	return QMetaType::fromName(name).id();
}

inline auto metaTypeFromVariant(const QVariant &variant) {
	return static_cast<QMetaType::Type>(variant.metaType().id());
}

inline auto stringSlice(const QString &str, qsizetype position) {
	return QStringView(str).sliced(position);
}

template <typename T>
inline T cast(T value) {
	return value;
}

template <typename T, typename U>
inline auto cast_6(U value) {
	return static_cast<T>(value);
}

template <typename T>
inline QString debug(T &&object) {
	return QDebug::toString(std::forward<T>(object));
}
#else
using NativeEventResult = long *;
using RetrieveDataMetaType = QVariant::Type;
using sizetype = int;

inline auto castSize(size_t size) {
	Q_ASSERT(size < std::numeric_limits<int>().max());
	return int(size);
}

inline auto isImageMime(const QString &, RetrieveDataMetaType type) {
	return type == QVariant::Image;
}

inline auto libraryPath(QLibraryInfo::LibraryLocation p) {
	return QLibraryInfo::location(p);
}

inline auto metaTypeFromName(const char *name) {
	return QVariant::nameToType(name);
}

inline auto metaTypeFromVariant(const QVariant &variant) {
	return static_cast<QMetaType::Type>(variant.type());
}

inline auto stringSlice(const QString &str, int position) {
	return str.midRef(position);
}

template <typename T, typename U>
inline auto cast(U value) {
	return static_cast<T>(value);
}

template <typename T>
inline T cast_6(T value) {
	return value;
}

// SPDX-SnippetBegin
// SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// SDPX—SnippetName: QDebug::toString from Qt 6
template <typename T>
QString debug(T &&object) {
	QString buffer;
	QDebug stream(&buffer);
	stream.nospace() << std::forward<T>(object);
	return buffer;
}
// SPDX-SnippetEnd
#endif

} // namespace compat

#endif
