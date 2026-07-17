// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include "windows_video_encoder.h"
#include "save_video.h"
#include <dpcommon/atomic.h>
#include <dpcommon/threading.h>
}
#include <guiddef.h>
#include <string>
#include <windows.h>
// This has to come first.
#include <mfidl.h>
// Then these.
#include <initguid.h>
#include <mfapi.h>
#include <mfreadwrite.h>


// GUIDs replicated from mfapi.h to avoid linking at build time.
DEFINE_GUID(DP_MF_MT_AVG_BITRATE, 0x20332624, 0xfb0d, 0x4d9e, 0xbd, 0x0d, 0xcb,
            0xf6, 0x78, 0x6c, 0x10, 0x2e);
DEFINE_GUID(DP_MF_MT_FRAME_RATE, 0xc459a2e8, 0x3d2c, 0x4e44, 0xb1, 0x32, 0xfe,
            0xe5, 0x15, 0x6c, 0x7b, 0xb0);
DEFINE_GUID(DP_MF_MT_FRAME_SIZE, 0x1652c33d, 0xd6b2, 0x4012, 0xb8, 0x34, 0x72,
            0x03, 0x08, 0x49, 0xa3, 0x7d);
DEFINE_GUID(DP_MF_MT_INTERLACE_MODE, 0xe2724bb8, 0xe676, 0x4806, 0xb4, 0xb2,
            0xa8, 0xd6, 0xef, 0xb4, 0x4c, 0xcd);
DEFINE_GUID(DP_MF_MT_MAJOR_TYPE, 0x48eba18e, 0xf8c9, 0x4687, 0xbf, 0x11, 0x0a,
            0x74, 0xc9, 0xf9, 0x6a, 0x8f);
DEFINE_GUID(DP_MF_MT_PIXEL_ASPECT_RATIO, 0xc6376a1e, 0x8d0a, 0x4027, 0xbe, 0x45,
            0x6d, 0x9a, 0x0a, 0xd3, 0x9b, 0xb6);
DEFINE_GUID(DP_MF_MT_SUBTYPE, 0xf7e34c9a, 0x42e8, 0x4714, 0xb7, 0x4b, 0xcb,
            0x29, 0xd7, 0x2c, 0x35, 0xe5);
DEFINE_GUID(DP_MFT_CATEGORY_VIDEO_ENCODER, 0xf79eac7d, 0xe545, 0x4387, 0xbd,
            0xee, 0xd6, 0x47, 0xd7, 0xbd, 0xe4, 0x2a);

DEFINE_GUID(DP_MFMediaType_Video, 0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00,
            0xAA, 0x00, 0x38, 0x9B, 0x71);

DEFINE_MEDIATYPE_GUID(DP_MFVideoFormat_AV1, FCC('AV01'));
DEFINE_MEDIATYPE_GUID(DP_MFVideoFormat_H264, FCC('H264'));
DEFINE_MEDIATYPE_GUID(DP_MFVideoFormat_NV12, FCC('NV12'));


#define HR_DO_WARN(STMT, HR, MSG)                         \
    do {                                                  \
        HRESULT _hr = (HR);                               \
        if (FAILED(_hr)) {                                \
            const char *_msg = (MSG);                     \
            DP_warn(_msg, static_cast<unsigned int>(_hr), \
                    hr_str(_hr).c_str());                 \
            STMT                                          \
        }                                                 \
    } while (0)

#define HR_WARN(HR, MSG) HR_DO_WARN(, HR, MSG)

#define HR_WARN_RETURN_FALSE(HR, MSG) HR_DO_WARN({ return false; }, HR, MSG)

#define HR_DO_CHECK(STMT, HR, MSG)                             \
    do {                                                       \
        HRESULT _hr = (HR);                                    \
        if (FAILED(_hr)) {                                     \
            const char *_msg = (MSG);                          \
            DP_error_set(_msg, static_cast<unsigned int>(_hr), \
                         hr_str(_hr).c_str());                 \
            STMT                                               \
        }                                                      \
    } while (0)

#define HR_CHECK_RETURN_FALSE(HR, MSG) \
    HR_DO_CHECK({ return false; }, (HR), (MSG));

#define HR_CHECK_RETURN_NULL(HR, MSG) \
    HR_DO_CHECK({ return nullptr; }, (HR), (MSG));


static std::string hr_str(HRESULT hr)
{
    using std::literals::string_literals::operator""s;

    LPWSTR wbuf = nullptr;
    DWORD size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM
            | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&wbuf), 0, nullptr);

    if (size == 0 || !wbuf) {
        return "Unknown error"s;
    }

    int required_capacity =
        WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, nullptr, 0, nullptr, nullptr);
    if (required_capacity <= 1) {
        return "Invalid error"s;
    }

    std::string buf(required_capacity - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, &buf[0], required_capacity,
                        nullptr, nullptr);
    return buf;
}


struct DP_WindowsMediaFoundation final {
    DP_CXX_DISABLE_COPY_MOVE(DP_WindowsMediaFoundation)
  public:
    using MfStartupFn = HRESULT(WINAPI *)(ULONG, DWORD);
    using MfShutdownFn = HRESULT(WINAPI *)();
    using MftEnumExFn = HRESULT(WINAPI *)(GUID, UINT32,
                                          const MFT_REGISTER_TYPE_INFO *,
                                          const MFT_REGISTER_TYPE_INFO *,
                                          IMFActivate ***, UINT32 *);
    using MfCreateMediaTypeFn = HRESULT(WINAPI *)(IMFMediaType **);
    using MfCreateSampleFn = HRESULT(WINAPI *)(IMFSample **);
    using MfCreate2dMediaBufferFn = HRESULT(WINAPI *)(DWORD, DWORD, DWORD, BOOL,
                                                      IMFMediaBuffer **);
    using MfCreateSinkWriterFromUrlFn = HRESULT(WINAPI *)(LPCWSTR,
                                                          IMFByteStream *,
                                                          IMFAttributes *,
                                                          IMFSinkWriter **);

    static DP_WindowsMediaFoundation *acquire()
    {
        DP_ATOMIC_DECLARE_STATIC_SPIN_LOCK(windows_media_encoder_spinlock);
        if (!mutex) {
            DP_atomic_lock(&windows_media_encoder_spinlock);
            if (!mutex) {
                mutex = DP_mutex_new();
            }
            DP_atomic_unlock(&windows_media_encoder_spinlock);
        }

        if (!mutex) {
            return nullptr;
        }

        DP_MUTEX_MUST_LOCK(mutex);
        DP_WindowsMediaFoundation *wmf = get_instance();
        DP_MUTEX_MUST_UNLOCK(mutex);
        return wmf;
    }

    static void release(DP_WindowsMediaFoundation *wmf)
    {
        DP_ASSERT(wmf);
        DP_ASSERT(instance == wmf);
        DP_ASSERT(mutex);

        DP_MUTEX_MUST_LOCK(mutex);
        if (wmf->decref()) {
            instance = nullptr;
            delete wmf;
        }
        DP_MUTEX_MUST_UNLOCK(mutex);
    }

    ~DP_WindowsMediaFoundation()
    {
        if (m_started) {
            DP_ASSERT(m_shutdown);
            HRESULT hr = m_shutdown();
            HR_WARN(hr, "Error %x shutting down Windows Media Foundation: %s");
        }
        FreeLibrary(m_mfReadWrite);
        FreeLibrary(m_mfPlat);
    }

    bool check_support(const GUID &type_guid)
    {
        MFT_REGISTER_TYPE_INFO output_type_info;
        output_type_info.guidMajorType = MFMediaType_Video;
        output_type_info.guidSubtype = type_guid;

        UINT32 count = 0;
        IMFActivate **activates = nullptr;
        HRESULT hr = m_enum_ex(DP_MFT_CATEGORY_VIDEO_ENCODER,
                               MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT
                                   | MFT_ENUM_FLAG_HARDWARE
                                   | MFT_ENUM_FLAG_TRANSCODE_ONLY
                                   | MFT_ENUM_FLAG_SORTANDFILTER,
                               nullptr, &output_type_info, &activates, &count);
        HR_WARN_RETURN_FALSE(hr, "Error %x enumerating video encoders: %s");

        for (UINT32 i = 0; i < count; ++i) {
            IMFActivate *activate = activates[i];
            if (activate) {
                activate->Release();
            }
        }
        CoTaskMemFree(activates);

        return count > 0;
    }

    IMFSinkWriter *create_writer(const char *path)
    {
        if (!path) {
            DP_error_set("No writer path given");
            return nullptr;
        }

        int required_capacity =
            MultiByteToWideChar(CP_UTF8, 0, path, -1, nullptr, 0);
        if (required_capacity <= 1) {
            DP_error_set("Writer path is empty");
            return nullptr;
        }

        std::wstring url(required_capacity - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, path, -1, &url[0], required_capacity);

        IMFSinkWriter *writer = nullptr;
        HRESULT hr = m_create_sink_writer_from_url(url.c_str(), nullptr,
                                                   nullptr, &writer);
        HR_CHECK_RETURN_NULL(hr, "Error %x creating sink writer: %s");
        return writer;
    }

    IMFMediaType *create_media_type()
    {
        IMFMediaType *type = nullptr;
        HRESULT hr = m_create_media_type(&type);
        HR_CHECK_RETURN_NULL(hr, "Error %x creating media type: %s");
        return type;
    }

    IMFSample *create_sample()
    {
        IMFSample *sample = nullptr;
        HRESULT hr = m_create_sample(&sample);
        HR_CHECK_RETURN_NULL(hr, "Error %x creating sample: %s");
        return sample;
    }

    IMFMediaBuffer *create_2d_media_buffer(DWORD width, DWORD height,
                                           DWORD four_cc, BOOL bottom_up)
    {
        IMFMediaBuffer *media_buffer = nullptr;
        HRESULT hr = m_create_2d_media_buffer(width, height, four_cc, bottom_up,
                                              &media_buffer);
        HR_CHECK_RETURN_NULL(hr, "Error %x creating 2D media buffer: %s");
        return media_buffer;
    }

  private:
    DP_WindowsMediaFoundation() = default;

    static DP_WindowsMediaFoundation *get_instance()
    {
        if (instance) {
            return instance->incref();
        }
        else {
            DP_WindowsMediaFoundation *wmf = new DP_WindowsMediaFoundation;
            if (wmf->load()) {
                instance = wmf;
                return wmf;
            }
            else {
                delete wmf;
                return nullptr;
            }
        }
    }

    bool load()
    {
        m_mfPlat = LoadLibraryA("mfplat.dll");
        if (!m_mfPlat) {
            DP_error_set("Failed to load mfplat.dll");
            return false;
        }

        m_mfReadWrite = LoadLibraryA("mfreadwrite.dll");
        if (!m_mfReadWrite) {
            DP_error_set("Failed to load mfreadwrite.dll");
            return false;
        }

        m_startup = reinterpret_cast<MfStartupFn>(
            GetProcAddress(m_mfPlat, "MFStartup"));
        if (!m_startup) {
            DP_error_set("Failed to get MFStartup proc address");
            return false;
        }

        m_shutdown = reinterpret_cast<MfShutdownFn>(
            GetProcAddress(m_mfPlat, "MFShutdown"));
        if (!m_shutdown) {
            DP_error_set("Failed to get MFShutdown proc address");
            return false;
        }

        m_enum_ex = reinterpret_cast<MftEnumExFn>(
            GetProcAddress(m_mfPlat, "MFTEnumEx"));
        if (!m_enum_ex) {
            DP_error_set("Failed to get MFTEnumEx proc address");
            return false;
        }

        m_create_media_type = reinterpret_cast<MfCreateMediaTypeFn>(
            GetProcAddress(m_mfPlat, "MFCreateMediaType"));
        if (!m_create_media_type) {
            DP_error_set("Failed to get MFCreateMediaType proc address");
            return false;
        }

        m_create_sample = reinterpret_cast<MfCreateSampleFn>(
            GetProcAddress(m_mfPlat, "MFCreateSample"));
        if (!m_create_sample) {
            DP_error_set("Failed to get MFCreateSample proc address");
            return false;
        }

        m_create_2d_media_buffer = reinterpret_cast<MfCreate2dMediaBufferFn>(
            GetProcAddress(m_mfPlat, "MFCreate2DMediaBuffer"));
        if (!m_create_2d_media_buffer) {
            DP_error_set("Failed to get MFCreate2DMediaBuffer proc address");
            return false;
        }

        m_create_sink_writer_from_url =
            reinterpret_cast<MfCreateSinkWriterFromUrlFn>(
                GetProcAddress(m_mfReadWrite, "MFCreateSinkWriterFromURL"));
        if (!m_create_sink_writer_from_url) {
            DP_error_set(
                "Failed to get MFCreateSinkWriterFromURL proc address");
            return false;
        }

        HRESULT hr = m_startup(MF_VERSION, MFSTARTUP_FULL);
        HR_CHECK_RETURN_FALSE(
            hr, "Error %x starting up Windows Media Foundation: %s");

        m_started = true;
        return true;
    }

    DP_WindowsMediaFoundation *incref()
    {
        ++m_refcount;
        return this;
    }

    bool decref()
    {
        return --m_refcount == 0;
    }

    HMODULE m_mfPlat = nullptr;
    HMODULE m_mfReadWrite = nullptr;
    MfStartupFn m_startup = nullptr;
    MfShutdownFn m_shutdown = nullptr;
    MftEnumExFn m_enum_ex = nullptr;
    MfCreateMediaTypeFn m_create_media_type = nullptr;
    MfCreateSampleFn m_create_sample = nullptr;
    MfCreate2dMediaBufferFn m_create_2d_media_buffer = nullptr;
    MfCreateSinkWriterFromUrlFn m_create_sink_writer_from_url = nullptr;
    bool m_started = false;
    int m_refcount = 1;
    static DP_Mutex *mutex;
    static DP_WindowsMediaFoundation *instance;
};

DP_Mutex *DP_WindowsMediaFoundation::mutex;
DP_WindowsMediaFoundation *DP_WindowsMediaFoundation::instance;

struct DP_WindowsVideoEncoder {
    DP_CXX_DISABLE_COPY_MOVE(DP_WindowsVideoEncoder)
  public:
    DP_WindowsVideoEncoder(DP_WindowsMediaFoundation *wmf) : m_wmf(wmf)
    {
    }

    ~DP_WindowsVideoEncoder()
    {
        if (m_writer) {
            if (m_writing) {
                HRESULT hr = m_writer->Finalize();
                HR_WARN(hr, "Error %x finalizing writer: %s");
            }
            m_writer->Release();
        }
        if (m_2d_buffer) {
            if (m_2d_buffer_locked) {
                HRESULT hr = m_2d_buffer->Unlock2D();
                HR_WARN(hr, "Error %x unlocking 2D buffer: %s");
            }
            m_2d_buffer->Release();
        }
        if (m_media_buffer) {
            m_media_buffer->Release();
        }
        if (m_sample) {
            m_sample->Release();
        }
        if (m_video_input_type) {
            m_video_input_type->Release();
        }
        if (m_video_output_type) {
            m_video_output_type->Release();
        }
        DP_WindowsMediaFoundation::release(m_wmf);
    }

    bool start(const DP_WindowsVideoEncoderParams *params)
    {
        m_writer = m_wmf->create_writer(params->path);
        if (!m_writer) {
            return false;
        }

        m_video_output_type = m_wmf->create_media_type();
        if (!m_video_output_type) {
            return false;
        }

        HRESULT hr = m_video_output_type->SetGUID(DP_MF_MT_MAJOR_TYPE,
                                                  DP_MFMediaType_Video);
        HR_CHECK_RETURN_FALSE(hr, "Error %x setting video output type: %s");

        hr = m_video_output_type->SetGUID(DP_MF_MT_SUBTYPE,
                                          DP_MFVideoFormat_H264);
        HR_CHECK_RETURN_FALSE(hr, "Error %x setting video output subtype: %s");

        hr = m_video_output_type->SetUINT32(DP_MF_MT_AVG_BITRATE, 6000000);
        HR_CHECK_RETURN_FALSE(hr, "Error %x setting video output bitrate: %s");

        hr = MFSetAttributeSize(m_video_output_type, DP_MF_MT_FRAME_SIZE,
                                UINT32(params->width), UINT32(params->height));
        HR_CHECK_RETURN_FALSE(hr,
                              "Error %x setting video output dimensions: %s");

        hr = MFSetAttributeRatio(m_video_output_type, DP_MF_MT_FRAME_RATE,
                                 UINT32(params->framerate_num),
                                 UINT32(params->framerate_den));
        HR_CHECK_RETURN_FALSE(hr,
                              "Error %x setting video output framerate: %s");

        hr = MFSetAttributeRatio(m_video_output_type,
                                 DP_MF_MT_PIXEL_ASPECT_RATIO, UINT32(1),
                                 UINT32(1));
        HR_CHECK_RETURN_FALSE(hr,
                              "Error %x setting video output pixel ratio: %s");

        hr = m_video_output_type->SetUINT32(DP_MF_MT_INTERLACE_MODE,
                                            MFVideoInterlace_Progressive);
        HR_CHECK_RETURN_FALSE(
            hr, "Error %x setting video output interlace mode: %s");

        hr = m_writer->AddStream(m_video_output_type, &m_video_stream_index);
        HR_CHECK_RETURN_FALSE(hr, "Error %x adding video output bitrate: %s");

        m_video_input_type = m_wmf->create_media_type();
        if (!m_video_input_type) {
            return false;
        }

        hr = m_video_input_type->SetGUID(DP_MF_MT_MAJOR_TYPE,
                                         DP_MFMediaType_Video);
        HR_CHECK_RETURN_FALSE(hr, "Error %x setting video input type: %s");

        hr = m_video_input_type->SetGUID(DP_MF_MT_SUBTYPE,
                                         DP_MFVideoFormat_NV12);
        HR_CHECK_RETURN_FALSE(hr, "Error %x setting video input subtype: %s");

        hr = MFSetAttributeSize(m_video_input_type, DP_MF_MT_FRAME_SIZE,
                                UINT32(params->width), UINT32(params->height));
        HR_CHECK_RETURN_FALSE(hr,
                              "Error %x setting video input dimensions: %s");

        hr = MFSetAttributeRatio(m_video_input_type, DP_MF_MT_FRAME_RATE,
                                 UINT32(params->framerate_num),
                                 UINT32(params->framerate_den));
        HR_CHECK_RETURN_FALSE(hr, "Error %x setting video input framerate: %s");

        hr =
            MFSetAttributeRatio(m_video_input_type, DP_MF_MT_PIXEL_ASPECT_RATIO,
                                UINT32(1), UINT32(1));
        HR_CHECK_RETURN_FALSE(hr,
                              "Error %x setting video input pixel ratio: %s");

        hr = m_writer->SetInputMediaType(m_video_stream_index,
                                         m_video_input_type, nullptr);
        HR_CHECK_RETURN_FALSE(hr, "Error %x setting input media type: %s");

        m_frame_duration =
            LONGLONG((10000000ULL * ULONGLONG(params->framerate_den))
                     / ULONGLONG(params->framerate_num));
        m_width = DWORD(params->width);
        m_height = DWORD(params->height);
        m_frame_size = DWORD(params->frame_size);
        return true;
    }

    bool start()
    {
        HRESULT hr = m_writer->BeginWriting();
        HR_CHECK_RETURN_FALSE(hr, "Error %x starting writer: %s");
        m_writing = true;
        return true;
    }

    bool prepare(DP_WindowsVideoEncoderFrame *out_frame)
    {
        DP_ASSERT(!m_sample);
        DP_ASSERT(!m_media_buffer);
        DP_ASSERT(!m_2d_buffer);
        DP_ASSERT(!m_2d_buffer_locked);

        m_sample = m_wmf->create_sample();
        if (!m_sample) {
            return false;
        }

        m_media_buffer = m_wmf->create_2d_media_buffer(
            m_width, m_height, DP_MFVideoFormat_NV12.Data1, FALSE);
        if (!m_media_buffer) {
            return false;
        }

        HRESULT hr = m_sample->AddBuffer(m_media_buffer);
        HR_CHECK_RETURN_FALSE(hr, "Error %x adding 2D media buffer: %s");

        m_2d_buffer = nullptr;
        hr = m_media_buffer->QueryInterface(IID_PPV_ARGS(&m_2d_buffer));
        HR_CHECK_RETURN_FALSE(hr, "Error %x querying 2D buffer interface: %s");

        BYTE *first_scanline = nullptr;
        LONG pitch = 0;
        hr = m_2d_buffer->Lock2D(&first_scanline, &pitch);
        HR_CHECK_RETURN_FALSE(hr, "Error %x locking 2D buffer: %s");
        m_2d_buffer_locked = true;

        DP_WindowsVideoEncoderFrame frame = {
            {
                first_scanline,
                first_scanline + (pitch * LONG(m_height)),
                nullptr,
                nullptr,
            },
            {
                int(pitch),
                int(pitch),
                0,
                0,
            },
        };
        *out_frame = frame;
        return true;
    }

    bool commit()
    {
        DP_ASSERT(m_sample);
        DP_ASSERT(m_media_buffer);
        DP_ASSERT(m_2d_buffer);
        DP_ASSERT(m_2d_buffer_locked);

        HRESULT hr = m_2d_buffer->Unlock2D();
        m_2d_buffer_locked = false;
        HR_CHECK_RETURN_FALSE(hr, "Error %x unlocking 2D buffer: %s");

        hr = m_media_buffer->SetCurrentLength(m_width * m_height * DWORD(3)
                                              / DWORD(2));
        HR_CHECK_RETURN_FALSE(hr, "Error %x setting media buffer length: %s");

        hr = m_sample->SetSampleTime(m_frame_index * m_frame_duration);
        HR_CHECK_RETURN_FALSE(hr, "Error %x setting sample time: %s");

        hr = m_sample->SetSampleDuration(m_frame_duration);
        HR_CHECK_RETURN_FALSE(hr, "Error %x setting sample duration: %s");

        hr = m_writer->WriteSample(m_video_stream_index, m_sample);
        HR_CHECK_RETURN_FALSE(hr, "Error %x writing sample: %s");

        m_2d_buffer->Release();
        m_2d_buffer = nullptr;

        m_media_buffer->Release();
        m_media_buffer = nullptr;

        m_sample->Release();
        m_sample = nullptr;

        ++m_frame_index;
        return true;
    }

    bool finish()
    {
        DP_ASSERT(m_writer);
        DP_ASSERT(m_writing);

        HRESULT hr = m_writer->Finalize();
        m_writing = false;
        HR_CHECK_RETURN_FALSE(hr, "Error %x finalizing writer: %s");

        return true;
    }

  private:
    DP_WindowsMediaFoundation *const m_wmf;
    IMFSinkWriter *m_writer = nullptr;
    IMFMediaType *m_video_output_type = nullptr;
    IMFMediaType *m_video_input_type = nullptr;
    IMFSample *m_sample = nullptr;
    IMFMediaBuffer *m_media_buffer = nullptr;
    IMF2DBuffer *m_2d_buffer = nullptr;
    LONGLONG m_frame_duration = 0LL;
    LONGLONG m_frame_index = 0LL;
    DWORD m_video_stream_index = 0;
    DWORD m_width = 0;
    DWORD m_height = 0;
    DWORD m_frame_size = 0;
    bool m_writing = false;
    bool m_2d_buffer_locked = false;
};


extern "C" DP_WindowsMediaFoundation *DP_windows_media_foundation_acquire(void)
{
    return DP_WindowsMediaFoundation::acquire();
}

extern "C" void
DP_windows_media_foundation_release(DP_WindowsMediaFoundation *wmf)
{
    if (wmf) {
        DP_WindowsMediaFoundation::release(wmf);
    }
}


static bool check_format_support(const GUID &type_guid)
{
    DP_WindowsMediaFoundation *wmf = DP_WindowsMediaFoundation::acquire();
    if (wmf) {
        bool supported = wmf->check_support(type_guid);
        DP_WindowsMediaFoundation::release(wmf);
        return supported;
    }
    else {
        DP_warn("Failed to acquire Windows Media Foundation to check format "
                "support: %s",
                DP_error());
        return false;
    }
}

extern "C" bool DP_windows_video_encoder_format_support(int format)
{
    switch (format) {
    case int(DP_SAVE_VIDEO_FORMAT_MP4_H264):
        return check_format_support(DP_MFVideoFormat_H264);
    case int(DP_SAVE_VIDEO_FORMAT_MP4_AV1):
        return check_format_support(DP_MFVideoFormat_AV1);
    default:
        // Support for other formats is way too spotty to bother, we might end
        // up with weird third-party codecs that don't work or are unable to
        // write to MP4 or WEBM files. Just skip them and let libav handle it.
        return false;
    }
}


extern "C" DP_WindowsVideoEncoder *
DP_windows_video_encoder_new(DP_WindowsVideoEncoderParams params)
{
    DP_WindowsMediaFoundation *wmf = DP_WindowsMediaFoundation::acquire();
    if (!wmf) {
        return nullptr;
    }

    DP_WindowsVideoEncoder *wve = new DP_WindowsVideoEncoder(wmf);
    if (wve->start(&params)) {
        return wve;
    }
    else {
        delete wve;
        return nullptr;
    }
}

extern "C" void DP_windows_video_encoder_free(DP_WindowsVideoEncoder *wve)
{
    delete wve;
}

extern "C" bool DP_windows_video_encoder_start(DP_WindowsVideoEncoder *wve)
{
    DP_ASSERT(wve);
    return wve->start();
}

extern "C" bool
DP_windows_video_encoder_prepare(DP_WindowsVideoEncoder *wve,
                                 DP_WindowsVideoEncoderFrame *out_frame)
{
    DP_ASSERT(wve);
    DP_ASSERT(out_frame);
    return wve->prepare(out_frame);
}

extern "C" bool DP_windows_video_encoder_commit(DP_WindowsVideoEncoder *wve)
{
    DP_ASSERT(wve);
    return wve->commit();
}

extern "C" bool DP_windows_video_encoder_finish(DP_WindowsVideoEncoder *wve)
{
    DP_ASSERT(wve);
    return wve->finish();
}
