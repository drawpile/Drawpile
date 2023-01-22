#ifndef DP_LIBSHARED_UTIL_QTCOMPAT_H
#define DP_LIBSHARED_UTIL_QTCOMPAT_H

#include <QtGlobal>
#include <QAbstractSocket>
#include <QLibraryInfo>
#include <QVariant>

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
using RetrieveDataMetaType = QMetaType;

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
#else
#define Q_MOC_INCLUDE(moc)

using RetrieveDataMetaType = QVariant::Type;

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
#endif

}

#endif
