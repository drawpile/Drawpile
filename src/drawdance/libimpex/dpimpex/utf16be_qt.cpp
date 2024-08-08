// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include "utf16be.h"
#include <dpcommon/common.h>
}
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#    include <QStringDecoder>
#    include <QStringEncoder>
using ByteArraySizeType = qsizetype;
#else
#    include <QTextCodec>
using ByteArraySizeType = int;
#endif


extern "C" bool DP_utf8_to_utf16be(const char *s,
                                   bool (*set_utf16be)(void *, const char *,
                                                       size_t),
                                   void *user)
{
    if (s) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QStringEncoder encoder(QStringEncoder::Utf16BE,
                               QStringConverter::Flag::Stateless);
        bool encoder_ok = encoder.isValid();
#else
        QTextCodec *encoder = QTextCodec::codecForName("UTF-16BE");
        bool encoder_ok = encoder != nullptr;
#endif
        if (encoder_ok) {
            QString input = QString::fromUtf8(s);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QByteArray result = encoder.encode(input);
#else
            QByteArray result = encoder->fromUnicode(input);
#endif
            return set_utf16be(user, result.constData(),
                               size_t(result.length()));
        }
    }
    return false;
}


static size_t utf16be_length(const uint16_t *s)
{
    size_t i = 0;
    while (s[i] != 0) {
        ++i;
    }
    return i;
}

extern "C" bool
DP_utf16be_to_utf8(const uint16_t *s,
                   bool (*set_utf8)(void *, const char *, size_t), void *user)
{
    if (s) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        QStringDecoder decoder(QStringDecoder::Utf16BE,
                               QStringConverter::Flag::Stateless);
        bool decoder_ok = decoder.isValid();
#else
        QTextCodec *decoder = QTextCodec::codecForName("UTF-16BE");
        bool decoder_ok = decoder != nullptr;
#endif
        if (decoder_ok) {
            QByteArray input = QByteArray::fromRawData(
                reinterpret_cast<const char *>(s),
                ByteArraySizeType(utf16be_length(s) * 2));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QByteArray result = QString(decoder(input)).toUtf8();
#else
            QByteArray result = decoder->toUnicode(input).toUtf8();
#endif
            return set_utf8(user, result.constData(), size_t(result.length()));
        }
    }
    return false;
}
