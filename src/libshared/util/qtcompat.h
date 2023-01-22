#ifndef DP_LIBSHARED_UTIL_QTCOMPAT_H
#define DP_LIBSHARED_UTIL_QTCOMPAT_H

#include <QtGlobal>
#include <QAbstractSocket>

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

inline auto stringSlice(const QString &str, qsizetype position) {
	return QStringView(str).sliced(position);
}
#else
#define Q_MOC_INCLUDE(moc)

using RetrieveDataMetaType = QVariant::Type;

inline auto isImageMime(const QString &, RetrieveDataMetaType type) {
	return type == QVariant::Image;
}

inline auto stringSlice(const QString &str, int position) {
	return str.midRef(position);
}
#endif

}

#endif
