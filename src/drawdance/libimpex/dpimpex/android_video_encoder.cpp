// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include "android_video_encoder.h"
}
#include <QByteArray>
#include <QFile>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#    include <QJniEnvironment>
#    include <QJniObject>
#else
#    include <QAndroidJniEnvironment>
#    include <QAndroidJniObject>
using QJniEnvironment = QAndroidJniEnvironment;
using QJniObject = QAndroidJniObject;
#endif


struct DP_AndroidVideoEncoder {
    QJniObject *encoder;
    QString output_path;
    QString temp_path;
};

namespace {
static bool clear_exception(QJniEnvironment &env)
{
    if (env->ExceptionCheck()) {
        DP_warn("JNI exception occurred");
        env->ExceptionDescribe();
        env->ExceptionClear();
        return true;
    }
    else {
        return false;
    }
}

static bool check_valid(const char *name, QJniObject &obj)
{
    if (obj.isValid()) {
        return true;
    }
    else {
        DP_warn("JNI object '%s' is not valid", name);
        return false;
    }
}

static bool is_error(int result)
{
    return result >= 100;
}
}

extern "C" void DP_android_video_encoder_format_support(
    int format, DP_AndroidVideoEncoderFormatSupportAddFn fn, void *user)
{
    QJniEnvironment env;
    QJniObject supports = QJniObject::callStaticObjectMethod(
        "net/drawpile/android/VideoEncoder", "getSupportsForFormat",
        "(I)Ljava/util/List;", jint(format));
    if (clear_exception(env) || !check_valid("supports", supports)) {
        return;
    }

    jint count = supports.callMethod<jint>("size", "()I");
    if (clear_exception(env)) {
        return;
    }

    for (jint i = 0; i < count; ++i) {
        QJniObject entry =
            supports.callObjectMethod("get", "(I)Ljava/lang/Object;", i);
        if (clear_exception(env) || !check_valid("entry", entry)) {
            continue;
        }

        QJniObject name_obj =
            entry.getObjectField("name", "Ljava/lang/String;");
        if (clear_exception(env) || !check_valid("name_obj", entry)) {
            continue;
        }

        QByteArray nameBytes = name_obj.toString().toUtf8();
        if (clear_exception(env) || nameBytes.isEmpty()) {
            continue;
        }

        bool hardware = entry.getField<jboolean>("hardware");
        if (clear_exception(env)) {
            continue;
        }

        fn(user, nameBytes.constData(), hardware);
    }
}

extern "C" DP_AndroidVideoEncoder *
DP_android_video_encoder_new(DP_AndroidVideoEncoderParams params)
{
    QJniEnvironment env;
    QString output_path = QString::fromUtf8(params.output);
    QJniObject output_obj = QJniObject::fromString(output_path);
    if (clear_exception(env) || !check_valid("output_obj", output_obj)) {
        return nullptr;
    }

    QString temp_path = QString::fromUtf8(params.temp);
    QJniObject temp_obj = QJniObject::fromString(temp_path);
    if (clear_exception(env) || !check_valid("temp_obj", temp_obj)) {
        return nullptr;
    }

    QJniObject encoder_obj;
    if (params.encoder && params.encoder[0] != '\0') {
        QString encoder = QString::fromUtf8(params.encoder);
        encoder_obj = QJniObject::fromString(encoder);
        clear_exception(env);
    }

    QJniObject *encoder = new QJniObject(
        "net/drawpile/android/VideoEncoder",
        "(IIIFLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        jint(params.format), jint(params.width), jint(params.height),
        jfloat(params.framerate), output_obj.object<jstring>(),
        temp_obj.object<jstring>(), encoder_obj.object<jstring>());
    if (clear_exception(env) || !check_valid("encoder", *encoder)) {
        delete encoder;
        return nullptr;
    }

    return new DP_AndroidVideoEncoder{encoder, output_path, temp_path};
}

extern "C" void DP_android_video_encoder_free(DP_AndroidVideoEncoder *ave)
{
    if (ave) {
        if (ave->encoder) {
            if (ave->encoder->isValid()) {
                QJniEnvironment env;
                ave->encoder->callMethod<void>("cancel", "()V");
                clear_exception(env);
            }
            delete ave->encoder;
        }
        delete ave;
    }
}

extern "C" int DP_android_video_encoder_start(DP_AndroidVideoEncoder *ave)
{
    DP_ASSERT(ave);
    DP_ASSERT(ave->encoder);

    QJniEnvironment env;
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt5/android/QtNative", "activity",
        "()Landroid/app/Activity;");
    if (clear_exception(env) || !check_valid("activity", activity)) {
        return -1;
    }

    int result = int(ave->encoder->callMethod<jint>(
        "start", "(Landroid/content/Context;)I", activity.object<jobject>()));
    if (clear_exception(env)) {
        return -1;
    }

    if (is_error(result)) {
        DP_error_set("Error %d in start", result);
    }
    return result;
}

extern "C" int DP_android_video_encoder_prepare(DP_AndroidVideoEncoder *ave,
                                                long long timeout_usec)
{
    DP_ASSERT(ave);
    DP_ASSERT(ave->encoder);

    QJniEnvironment env;
    int result = int(
        ave->encoder->callMethod<jint>("prepare", "(J)I", jlong(timeout_usec)));
    if (clear_exception(env)) {
        return -1;
    }

    if (is_error(result)) {
        DP_error_set("Error %d in prepare", result);
    }
    return result;
}

namespace {
static bool get_plane_buffer(QJniEnvironment &env, QJniObject &encoder,
                             int index, uint8_t **out_buffer)
{
    QJniObject plane = encoder.callObjectMethod(
        "getInputImagePlaneBuffer", "(I)Ljava/nio/ByteBuffer;", jint(index));
    if (clear_exception(env) || !check_valid("plane", plane)) {
        return false;
    }

    uint8_t *buffer = static_cast<uint8_t *>(
        env->GetDirectBufferAddress(plane.object<jobject>()));
    if (!buffer) {
        DP_error_set("No plane buffer at index %d", index);
        return false;
    }

    *out_buffer = buffer;
    return true;
}

static bool get_plane_row_stride(QJniEnvironment &env, QJniObject &encoder,
                                 int index, int *out_row_stride)
{
    jint row_stride = encoder.callMethod<jint>("getInputImagePlaneRowStride",
                                               "(I)I", jint(index));
    if (clear_exception(env)) {
        return false;
    }
    else if (row_stride <= 0) {
        DP_error_set("Invalid row stride %d at index %d", int(row_stride),
                     index);
        return false;
    }

    *out_row_stride = int(row_stride);
    return true;
}

static bool get_plane_pixel_stride(QJniEnvironment &env, QJniObject &encoder,
                                   int index, int *out_pixel_stride)
{
    jint pixel_stride = encoder.callMethod<jint>(
        "getInputImagePlanePixelStride", "(I)I", jint(index));
    if (clear_exception(env)) {
        return false;
    }
    else if (pixel_stride <= 0) {
        DP_error_set("Invalid pixel stride %d at index %d", int(pixel_stride),
                     index);
        return false;
    }

    *out_pixel_stride = int(pixel_stride);
    return true;
}
}

extern "C" bool
DP_android_video_encoder_image(DP_AndroidVideoEncoder *ave,
                               DP_AndroidVideoEncoderImage *out_image)
{
    DP_ASSERT(ave);
    DP_ASSERT(ave->encoder);
    DP_ASSERT(out_image);

    QJniEnvironment env;
    QJniObject &encoder = *ave->encoder;
    return get_plane_buffer(env, encoder, 0, &out_image->buffer_y)
        && get_plane_buffer(env, encoder, 1, &out_image->buffer_u)
        && get_plane_buffer(env, encoder, 2, &out_image->buffer_v)
        && get_plane_row_stride(env, encoder, 0, &out_image->row_stride_y)
        && get_plane_row_stride(env, encoder, 1, &out_image->row_stride_u)
        && get_plane_row_stride(env, encoder, 2, &out_image->row_stride_v)
        && get_plane_pixel_stride(env, encoder, 1, &out_image->pixel_stride_u)
        && get_plane_pixel_stride(env, encoder, 2, &out_image->pixel_stride_v);
}

extern "C" int DP_android_video_encoder_commit(DP_AndroidVideoEncoder *ave)
{
    DP_ASSERT(ave);
    DP_ASSERT(ave->encoder);

    QJniEnvironment env;
    int result = int(ave->encoder->callMethod<jint>("commit", "()I"));
    if (clear_exception(env)) {
        return -1;
    }

    if (is_error(result)) {
        DP_error_set("Error %d in commit", result);
    }
    return result;
}

extern "C" int DP_android_video_encoder_drain(DP_AndroidVideoEncoder *ave,
                                              long long timeout_usec)
{
    DP_ASSERT(ave);
    DP_ASSERT(ave->encoder);

    QJniEnvironment env;
    int result = int(
        ave->encoder->callMethod<jint>("drain", "(J)I", jlong(timeout_usec)));
    if (clear_exception(env)) {
        return -1;
    }

    if (is_error(result)) {
        DP_error_set("Error %d in drain", result);
    }
    return result;
}

extern "C" int DP_android_video_encoder_finish(DP_AndroidVideoEncoder *ave)
{
    DP_ASSERT(ave);
    DP_ASSERT(ave->encoder);

    QJniEnvironment env;
    int result = int(ave->encoder->callMethod<jint>("finish", "()I"));
    if (clear_exception(env)) {
        return -1;
    }

    if (is_error(result)) {
        DP_error_set("Error %d in finish", result);
    }
    return result;
}

namespace {
static bool copy_file_contents(const QString &temp_path,
                               const QString &output_path)
{
    QFile temp_file(temp_path);
    if (!temp_file.open(QIODevice::ReadOnly)) {
        DP_error_set("Failed to open temp file '%s': %s",
                     qUtf8Printable(temp_path),
                     qUtf8Printable(temp_file.errorString()));
        return false;
    }

    QFile output_file(output_path);
    if (!output_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        DP_error_set("Failed to open output file '%s': %s",
                     qUtf8Printable(output_path),
                     qUtf8Printable(output_file.errorString()));
        return false;
    }

    QByteArray buffer;
    buffer.resize(BUFSIZ);
    while (true) {
        long long read = temp_file.read(buffer.data(), BUFSIZ);
        if (read < 0) {
            DP_error_set("Error reading from temp file '%s': %s",
                         qUtf8Printable(temp_path),
                         qUtf8Printable(temp_file.errorString()));
            return false;
        }
        else if (read > 0) {
            long long written = output_file.write(buffer, read);
            if (written < 0) {
                DP_error_set(
                    "Error writing %lld byte(s) to output file '%s': %s", read,
                    qUtf8Printable(output_path),
                    qUtf8Printable(output_file.errorString()));
                return false;
            }
            else if (written != read) {
                DP_error_set("Tried to write %lld byte(s) to output file '%s', "
                             "but only wrote %lld",
                             read, qUtf8Printable(output_path), written);
                return false;
            }
        }
        else {
            if (output_file.flush()) {
                return true;
            }
            else {
                DP_error_set("Error flushing output file '%s': %s",
                             qUtf8Printable(output_path),
                             qUtf8Printable(output_file.errorString()));
                return false;
            }
        }
    }
}
}

extern "C" int DP_android_video_encoder_close(DP_AndroidVideoEncoder *ave)
{
    DP_ASSERT(ave);
    DP_ASSERT(ave->encoder);

    QJniEnvironment env;
    int result = int(ave->encoder->callMethod<jint>("close", "()I"));
    if (clear_exception(env)) {
        return -1;
    }

    if (result == DP_ANDROID_VIDEO_ENCODER_STATUS_NEEDS_COPY) {
        if (copy_file_contents(ave->temp_path, ave->output_path)) {
            return DP_ANDROID_VIDEO_ENCODER_STATUS_OK;
        }
        else {
            return DP_ANDROID_VIDEO_ENCODER_STATUS_ERROR_COPY;
        }
    }
    else {
        if (is_error(result)) {
            DP_error_set("Error %d in close", result);
        }
        return result;
    }
}
