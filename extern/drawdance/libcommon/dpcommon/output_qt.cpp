extern "C" {
#include "output_qt.h"
#include "output.h"
}

#include <QFile>
#include <QSaveFile>
#ifdef DP_QT_IO_KARCHIVE
#    include <KCompressionDevice>
#endif


static size_t do_write(const char *type, QIODevice *dev, const void *buffer,
                       size_t size)
{
    qint64 result = dev->write(static_cast<const char *>(buffer), qint64(size));
    size_t written = result < 0 ? 0 : size_t(result);
    if (written != size) {
        DP_error_set("%s output wrote %zu instead of expected %zu bytes: %s",
                     type, size, written, qUtf8Printable(dev->errorString()));
    }
    return written;
}

static bool do_flush(const char *type, QFileDevice *dev)
{
    if (dev->flush()) {
        return true;
    }
    else {
        DP_error_set("%s output flush error: %s", type,
                     qUtf8Printable(dev->errorString()));
        return false;
    }
}

static size_t do_tell(const char *type, QIODevice *dev, bool *out_error)
{
    qint64 offset = dev->pos();
    if (offset >= 0) {
        return size_t(offset);
    }
    else {
        DP_error_set("%s output tell error: %s", type,
                     qUtf8Printable(dev->errorString()));
        *out_error = true;
        return 0;
    }
}

static bool do_seek(const char *type, QIODevice *dev, size_t offset)
{
    if (dev->seek(qint64(offset))) {
        return true;
    }
    else {
        DP_error_set("%s output could not seek to %zu: %s", type, offset,
                     qUtf8Printable(dev->errorString()));
        return false;
    }
}


struct DP_QFileOutputState {
    QFile *file;
};

static QFile *get_file(void *internal)
{
    return static_cast<DP_QFileOutputState *>(internal)->file;
}

static size_t qfile_output_write(void *internal, const void *buffer,
                                 size_t size)
{
    return do_write("QFile", get_file(internal), buffer, size);
}

static bool qfile_output_flush(void *internal)
{
    return do_flush("QFile", get_file(internal));
}

static size_t qfile_output_tell(void *internal, bool *out_error)
{
    return do_tell("QFile", get_file(internal), out_error);
}

static bool qfile_output_seek(void *internal, size_t offset)
{
    return do_seek("QFile", get_file(internal), offset);
}

static bool qfile_output_dispose(void *internal)
{
    QFile *file = get_file(internal);
    bool ok = do_flush("QFile", file);
    delete file;
    return ok;
}

static const DP_OutputMethods qfile_output_methods = {
    qfile_output_write, NULL,
    qfile_output_flush, qfile_output_tell,
    qfile_output_seek,  qfile_output_dispose,
};

static const DP_OutputMethods *qfile_output_init(void *internal, void *arg)
{
    DP_QFileOutputState *state = static_cast<DP_QFileOutputState *>(internal);
    state->file = static_cast<QFile *>(arg);
    return &qfile_output_methods;
}

extern "C" DP_Output *DP_qfile_output_new_from_path(const char *path,
                                                    DP_OutputQtNewFn new_fn)
{
    QFile *file = new QFile{QString::fromUtf8(path)};
    if (file->open(QIODevice::WriteOnly)) {
        return new_fn(qfile_output_init, file, sizeof(DP_QFileOutputState));
    }
    else {
        DP_error_set("Can't open '%s': %s", path,
                     qUtf8Printable(file->errorString()));
        delete file;
        return NULL;
    }
}


struct DP_QSaveFileOutputState {
    QSaveFile *sf;
};

static QSaveFile *get_save_file(void *internal)
{
    return static_cast<DP_QSaveFileOutputState *>(internal)->sf;
}

static size_t qsavefile_output_write(void *internal, const void *buffer,
                                     size_t size)
{
    return do_write("QSaveFile", get_file(internal), buffer, size);
}

static bool qsavefile_output_flush(void *internal)
{
    return do_flush("QSaveFile", get_file(internal));
}

static size_t qsavefile_output_tell(void *internal, bool *out_error)
{
    return do_tell("QSaveFile", get_file(internal), out_error);
}

static bool qsavefile_output_seek(void *internal, size_t offset)
{
    return do_seek("QSaveFile", get_file(internal), offset);
}

static bool qsavefile_output_dispose(void *internal)
{
    QSaveFile *sf = get_save_file(internal);
    bool ok = sf->commit();
    delete sf;
    return ok;
}

static const DP_OutputMethods qsavefile_output_methods = {
    qsavefile_output_write, NULL,
    qsavefile_output_flush, qsavefile_output_tell,
    qsavefile_output_seek,  qsavefile_output_dispose,
};

static const DP_OutputMethods *qsavefile_output_init(void *internal, void *arg)
{
    DP_QSaveFileOutputState *state =
        static_cast<DP_QSaveFileOutputState *>(internal);
    state->sf = static_cast<QSaveFile *>(arg);
    return &qsavefile_output_methods;
}

extern "C" DP_Output *DP_qsavefile_output_new_from_path(const char *path,
                                                        DP_OutputQtNewFn new_fn)
{
    QSaveFile *sf = new QSaveFile{QString::fromUtf8(path)};
    sf->setDirectWriteFallback(true);
    if (sf->open(QIODevice::WriteOnly)) {
        return new_fn(qsavefile_output_init, sf, sizeof(DP_QFileOutputState));
    }
    else {
        DP_error_set("Can't open '%s': %s", path,
                     qUtf8Printable(sf->errorString()));
        delete sf;
        return NULL;
    }
}


#ifdef DP_QT_IO_KARCHIVE

struct DP_KCompressionDeviceOutputState {
    KCompressionDevice *dev;
};

static KCompressionDevice *get_dev(void *internal)
{
    return static_cast<DP_KCompressionDeviceOutputState *>(internal)->dev;
}

static size_t kcompressiondevice_output_write(void *internal,
                                              const void *buffer, size_t size)
{
    return do_write("KCompressionDevice", get_dev(internal), buffer, size);
}

static size_t kcompressiondevice_output_tell(void *internal, bool *out_error)
{
    return do_tell("KCompressionDevice", get_dev(internal), out_error);
}

static bool kcompressiondevice_output_seek(void *internal, size_t offset)
{
    return do_seek("KCompressionDevice", get_dev(internal), offset);
}

static bool kcompressiondevice_output_dispose(void *internal)
{
    KCompressionDevice *dev = get_dev(internal);
    dev->close();
    bool ok = dev->error() == QFileDevice::NoError;
    if (!ok) {
        DP_error_set("KCompressionDevice output flush error: %s",
                     qUtf8Printable(dev->errorString()));
    }
    delete dev;
    return ok;
}

static const DP_OutputMethods kcompressiondevice_output_methods = {
    kcompressiondevice_output_write,
    NULL,
    NULL,
    kcompressiondevice_output_tell,
    kcompressiondevice_output_seek,
    kcompressiondevice_output_dispose,
};

static const DP_OutputMethods *kcompressiondevice_output_init(void *internal,
                                                              void *arg)
{
    DP_KCompressionDeviceOutputState *state =
        static_cast<DP_KCompressionDeviceOutputState *>(internal);
    state->dev = static_cast<KCompressionDevice *>(arg);
    return &kcompressiondevice_output_methods;
}

extern "C" DP_Output *
DP_karchive_gzip_output_new_from_path(const char *path, DP_OutputQtNewFn new_fn)
{
    KCompressionDevice *dev = new KCompressionDevice{QString::fromUtf8(path),
                                                     KCompressionDevice::GZip};
    if (dev->open(QIODevice::WriteOnly)) {
        return new_fn(kcompressiondevice_output_init, dev,
                      sizeof(DP_KCompressionDeviceOutputState));
    }
    else {
        DP_error_set("Can't open '%s': %s", path,
                     qUtf8Printable(dev->errorString()));
        delete dev;
        return NULL;
    }
}

#endif
