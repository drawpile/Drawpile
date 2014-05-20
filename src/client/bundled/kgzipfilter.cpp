/* This file is part of the KDE libraries
   Copyright (C) 2000-2005 David Faure <faure@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kgzipfilter.h"

#include <time.h>
#include <zlib.h>
#include <QDebug>
#include <QtCore/QIODevice>

/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */

// #define DEBUG_GZIP

class KGzipFilter::Private
{
public:
    Private()
        : headerWritten(false), footerWritten(false), compressed(false), mode(0), crc(0), isInitialized(false)
    {
        zStream.zalloc = (alloc_func)0;
        zStream.zfree = (free_func)0;
        zStream.opaque = (voidpf)0;
    }

    z_stream zStream;
    bool headerWritten;
    bool footerWritten;
    bool compressed;
    int mode;
    ulong crc;
    bool isInitialized;
};

KGzipFilter::KGzipFilter()
    : d(new Private)
{
}

KGzipFilter::~KGzipFilter()
{
    delete d;
}

bool KGzipFilter::init(int mode)
{
    switch (filterFlags()) {
    case NoHeaders:
        return init(mode, RawDeflate);
        break;
    case WithHeaders:
        return init(mode, GZipHeader);
        break;
    case ZlibHeaders:
        return init(mode, ZlibHeader);
        break;
    }
    return false;
}

bool KGzipFilter::init(int mode, Flag flag)
{
    if (d->isInitialized) {
        terminate();
    }
    d->zStream.next_in = Z_NULL;
    d->zStream.avail_in = 0;
    if (mode == QIODevice::ReadOnly) {
        const int windowBits = (flag == RawDeflate)
                               ? -MAX_WBITS /*no zlib header*/
                               : (flag == GZipHeader) ?
                               MAX_WBITS + 32 /* auto-detect and eat gzip header */
                               : MAX_WBITS /*zlib header*/;
        const int result = inflateInit2(&d->zStream, windowBits);
        if (result != Z_OK) {
            //qDebug() << "inflateInit2 returned " << result;
            return false;
        }
    } else if (mode == QIODevice::WriteOnly) {
        int result = deflateInit2(&d->zStream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY); // same here
        if (result != Z_OK) {
            //qDebug() << "deflateInit returned " << result;
            return false;
        }
    } else {
        //qWarning() << "KGzipFilter: Unsupported mode " << mode << ". Only QIODevice::ReadOnly and QIODevice::WriteOnly supported";
        return false;
    }
    d->mode = mode;
    d->compressed = true;
    d->headerWritten = false;
    d->footerWritten = false;
    d->isInitialized = true;
    return true;
}

int KGzipFilter::mode() const
{
    return d->mode;
}

bool KGzipFilter::terminate()
{
    if (d->mode == QIODevice::ReadOnly) {
        int result = inflateEnd(&d->zStream);
        if (result != Z_OK) {
            //qDebug() << "inflateEnd returned " << result;
            return false;
        }
    } else if (d->mode == QIODevice::WriteOnly) {
        int result = deflateEnd(&d->zStream);
        if (result != Z_OK) {
            //qDebug() << "deflateEnd returned " << result;
            return false;
        }
    }
    d->isInitialized = false;
    return true;
}

void KGzipFilter::reset()
{
    if (d->mode == QIODevice::ReadOnly) {
        int result = inflateReset(&d->zStream);
        if (result != Z_OK) {
            //qDebug() << "inflateReset returned " << result;
            // TODO return false
        }
    } else if (d->mode == QIODevice::WriteOnly) {
        int result = deflateReset(&d->zStream);
        if (result != Z_OK) {
            //qDebug() << "deflateReset returned " << result;
            // TODO return false
        }
        d->headerWritten = false;
        d->footerWritten = false;
    }
}

bool KGzipFilter::readHeader()
{
    // We now rely on zlib to read the full header (see the MAX_WBITS + 32 in init).
    // We just use this method to check if the data is actually compressed.

#ifdef DEBUG_GZIP
    qDebug() << "avail=" << d->zStream.avail_in;
#endif
    // Assume not compressed until we see a gzip header
    d->compressed = false;
    Bytef *p = d->zStream.next_in;
    int i = d->zStream.avail_in;
    if ((i -= 10)  < 0) {
        return false;    // Need at least 10 bytes
    }
#ifdef DEBUG_GZIP
    qDebug() << "first byte is " << QString::number(*p, 16);
#endif
    if (*p++ != 0x1f) {
        return false;    // GZip magic
    }
#ifdef DEBUG_GZIP
    qDebug() << "second byte is " << QString::number(*p, 16);
#endif
    if (*p++ != 0x8b) {
        return false;
    }

#if 0
    int method = *p++;
    int flags = *p++;
    if ((method != Z_DEFLATED) || (flags & RESERVED) != 0) {
        return false;
    }
    p += 6;
    if ((flags & EXTRA_FIELD) != 0) { // skip extra field
        if ((i -= 2) < 0) {
            return false;    // Need at least 2 bytes
        }
        int len = *p++;
        len += (*p++) << 8;
        if ((i -= len) < 0) {
            return false;    // Need at least len bytes
        }
        p += len;
    }
    if ((flags & ORIG_NAME) != 0) { // skip original file name
#ifdef DEBUG_GZIP
        qDebug() << "ORIG_NAME=" << (char *)p;
#endif
        while ((i > 0) && (*p)) {
            i--; p++;
        }
        if (--i <= 0) {
            return false;
        }
        p++;
    }
    if ((flags & COMMENT) != 0) { // skip comment
        while ((i > 0) && (*p)) {
            i--; p++;
        }
        if (--i <= 0) {
            return false;
        }
        p++;
    }
    if ((flags & HEAD_CRC) != 0) { // skip the header crc
        if ((i -= 2) < 0) {
            return false;
        }
        p += 2;
    }

    d->zStream.avail_in = i;
    d->zStream.next_in = p;
#endif

    d->compressed = true;
#ifdef DEBUG_GZIP
    qDebug() << "header OK";
#endif
    return true;
}

/* Output a 16 bit value, lsb first */
#define put_short(w) \
    *p++ = (uchar) ((w) & 0xff); \
    *p++ = (uchar) ((ushort)(w) >> 8);

/* Output a 32 bit value to the bit stream, lsb first */
#define put_long(n) \
    put_short((n) & 0xffff); \
    put_short(((ulong)(n)) >> 16);

bool KGzipFilter::writeHeader(const QByteArray &fileName)
{
    Bytef *p = d->zStream.next_out;
    int i = d->zStream.avail_out;
    *p++ = 0x1f;
    *p++ = 0x8b;
    *p++ = Z_DEFLATED;
    *p++ = ORIG_NAME;
    put_long(time(0L));     // Modification time (in unix format)
    *p++ = 0; // Extra flags (2=max compress, 4=fastest compress)
    *p++ = 3; // Unix

    uint len = fileName.length();
    for (uint j = 0; j < len; ++j) {
        *p++ = fileName[j];
    }
    *p++ = 0;
    int headerSize = p - d->zStream.next_out;
    i -= headerSize;
    Q_ASSERT(i > 0);
    d->crc = crc32(0L, Z_NULL, 0);
    d->zStream.next_out = p;
    d->zStream.avail_out = i;
    d->headerWritten = true;
    return true;
}

void KGzipFilter::writeFooter()
{
    Q_ASSERT(d->headerWritten);
    Q_ASSERT(!d->footerWritten);
    Bytef *p = d->zStream.next_out;
    int i = d->zStream.avail_out;
    //qDebug() << "avail_out=" << i << "writing CRC=" << QString::number(d->crc, 16) << "at p=" << p;
    put_long(d->crc);
    //qDebug() << "writing totalin=" << d->zStream.total_in << "at p=" << p;
    put_long(d->zStream.total_in);
    i -= p - d->zStream.next_out;
    d->zStream.next_out = p;
    d->zStream.avail_out = i;
    d->footerWritten = true;
}

void KGzipFilter::setOutBuffer(char *data, uint maxlen)
{
    d->zStream.avail_out = maxlen;
    d->zStream.next_out = (Bytef *) data;
}
void KGzipFilter::setInBuffer(const char *data, uint size)
{
#ifdef DEBUG_GZIP
    qDebug() << "avail_in=" << size;
#endif
    d->zStream.avail_in = size;
    d->zStream.next_in = (Bytef *) data;
}
int KGzipFilter::inBufferAvailable() const
{
    return d->zStream.avail_in;
}
int KGzipFilter::outBufferAvailable() const
{
    return d->zStream.avail_out;
}

KGzipFilter::Result KGzipFilter::uncompress_noop()
{
    // I'm not sure we really need support for that (uncompressed streams),
    // but why not, it can't hurt to have it. One case I can think of is someone
    // naming a tar file "blah.tar.gz" :-)
    if (d->zStream.avail_in > 0) {
        int n = (d->zStream.avail_in < d->zStream.avail_out) ? d->zStream.avail_in : d->zStream.avail_out;
        memcpy(d->zStream.next_out, d->zStream.next_in, n);
        d->zStream.avail_out -= n;
        d->zStream.next_in += n;
        d->zStream.avail_in -= n;
        return KFilterBase::Ok;
    } else {
        return KFilterBase::End;
    }
}

KGzipFilter::Result KGzipFilter::uncompress()
{
#ifndef NDEBUG
    if (d->mode == 0) {
        //qWarning() << "mode==0; KGzipFilter::init was not called!";
        return KFilterBase::Error;
    } else if (d->mode == QIODevice::WriteOnly) {
        //qWarning() << "uncompress called but the filter was opened for writing!";
        return KFilterBase::Error;
    }
    Q_ASSERT(d->mode == QIODevice::ReadOnly);
#endif

    if (d->compressed) {
#ifdef DEBUG_GZIP
        qDebug() << "Calling inflate with avail_in=" << inBufferAvailable() << " avail_out=" << outBufferAvailable();
        qDebug() << "    next_in=" << d->zStream.next_in;
#endif
        int result = inflate(&d->zStream, Z_SYNC_FLUSH);
#ifdef DEBUG_GZIP
        qDebug() << " -> inflate returned " << result;
        qDebug() << " now avail_in=" << inBufferAvailable() << " avail_out=" << outBufferAvailable();
        qDebug() << "     next_in=" << d->zStream.next_in;
#else
        if (result != Z_OK && result != Z_STREAM_END) {
            //qDebug() << "Warning: inflate() returned " << result;
        }
#endif
        return (result == Z_OK ? KFilterBase::Ok : (result == Z_STREAM_END ? KFilterBase::End : KFilterBase::Error));
    } else {
        return uncompress_noop();
    }
}

KGzipFilter::Result KGzipFilter::compress(bool finish)
{
    Q_ASSERT(d->compressed);
    Q_ASSERT(d->mode == QIODevice::WriteOnly);

    Bytef *p = d->zStream.next_in;
    ulong len = d->zStream.avail_in;
#ifdef DEBUG_GZIP
    qDebug() << "  calling deflate with avail_in=" << inBufferAvailable() << " avail_out=" << outBufferAvailable();
#endif
    const int result = deflate(&d->zStream, finish ? Z_FINISH : Z_NO_FLUSH);
    if (result != Z_OK && result != Z_STREAM_END) {
        //qDebug() << "  deflate returned " << result;
    }
    if (d->headerWritten) {
        //qDebug() << "Computing CRC for the next " << len - d->zStream.avail_in << " bytes";
        d->crc = crc32(d->crc, p, len - d->zStream.avail_in);
    }
    KGzipFilter::Result callerResult = result == Z_OK ? KFilterBase::Ok : (Z_STREAM_END ? KFilterBase::End : KFilterBase::Error);

    if (result == Z_STREAM_END && d->headerWritten && !d->footerWritten) {
        if (d->zStream.avail_out >= 8 /*footer size*/) {
            //qDebug() << "finished, write footer";
            writeFooter();
        } else {
            // No room to write the footer (#157706/#188415), we'll have to do it on the next pass.
            //qDebug() << "finished, but no room for footer yet";
            callerResult = KFilterBase::Ok;
        }
    }
    return callerResult;
}
