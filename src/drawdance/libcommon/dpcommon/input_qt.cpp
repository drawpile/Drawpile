extern "C" {
#include "input_qt.h"
#include "input.h"
}

#include <QFile>


struct DP_QFileInputState {
    QFile *file;
    bool close;
};

QFile *get_file(void *internal)
{
    return static_cast<DP_QFileInputState *>(internal)->file;
}

static size_t qfile_input_read(void *internal, void *buffer, size_t size,
                               bool *out_error)
{
    QFile *file = get_file(internal);
    qint64 result = file->read(static_cast<char *>(buffer), qint64(size));
    if (result >= 0) {
        return size_t(result);
    }
    else {
        *out_error = true;
        DP_error_set("QFile input read error: %s",
                     qUtf8Printable(file->errorString()));
        return 0;
    }
}

static size_t qfile_input_length(void *internal, bool *out_error)
{
    QFile *file = get_file(internal);
    qint64 size = file->size();
    if (size >= 0) {
        return size_t(size);
    }
    else {
        *out_error = true;
        DP_error_set("QFile input size error: %s",
                     qUtf8Printable(file->errorString()));
        return 0;
    }
}

static bool qfile_input_rewind(void *internal)
{
    QFile *file = get_file(internal);
    if (file->seek(0)) {
        return true;
    }
    else {
        DP_error_set("QFile input could not rewind to beginning: %s",
                     qUtf8Printable(file->errorString()));
        return false;
    }
}

static bool qfile_input_rewind_by(void *internal, size_t size)
{
    QFile *file = get_file(internal);
    qint64 pos = file->pos();
    if (pos >= 0 && file->seek(pos - qint64(size))) {
        return true;
    }
    else {
        DP_error_set("File input could not rewind by %zu: %s", size,
                     qUtf8Printable(file->errorString()));
        return false;
    }
}

static bool qfile_input_seek(void *internal, size_t offset)
{
    QFile *file = get_file(internal);
    if (file->seek(qint64(offset))) {
        return true;
    }
    else {
        DP_error_set("File input could not seek to %zu: %s", offset,
                     qUtf8Printable(file->errorString()));
        return false;
    }
}

static bool qfile_input_seek_by(void *internal, size_t size)
{
    QFile *file = get_file(internal);
    qint64 pos = file->pos();
    if (pos >= 0) {
        qint64 target = pos + qint64(size);
        if (target <= file->size() && file->seek(target)) {
            return true;
        }
    }
    DP_error_set("File input could not seek by %zu: %s", size,
                 qUtf8Printable(file->errorString()));
    return false;
}

static QIODevice *qfile_input_qiodevice(void *internal)
{
    return get_file(internal);
}

static void qfile_input_dispose(void *internal)
{
    DP_QFileInputState *state = static_cast<DP_QFileInputState *>(internal);
    if (state->close) {
        QFile *file = state->file;
        file->close();
        delete file;
    }
}

static const DP_InputMethods qfile_input_methods = {
    qfile_input_read,      qfile_input_length,  qfile_input_rewind,
    qfile_input_rewind_by, qfile_input_seek,    qfile_input_seek_by,
    qfile_input_qiodevice, qfile_input_dispose,
};

const DP_InputMethods *qfile_input_init(void *internal, void *arg)
{
    DP_QFileInputState *state = static_cast<DP_QFileInputState *>(internal);
    *state = *static_cast<DP_QFileInputState *>(arg);
    return &qfile_input_methods;
}

extern "C" DP_Input *DP_qfile_input_new(QFile *file, bool close,
                                        DP_InputQtNewFn new_fn)
{
    DP_QFileInputState state = {file, close};
    return new_fn(qfile_input_init, &state, sizeof(DP_QFileInputState));
}

extern "C" DP_Input *DP_qfile_input_new_from_path(const char *path,
                                                  DP_InputQtNewFn new_fn)
{
    QFile *file = new QFile{QString::fromUtf8(path)};
    if (file->open(QIODevice::ReadOnly)) {
        return DP_qfile_input_new(file, true, new_fn);
    }
    else {
        DP_error_set("Can't open '%s': %s", path,
                     qUtf8Printable(file->errorString()));
        delete file;
        return nullptr;
    }
}
