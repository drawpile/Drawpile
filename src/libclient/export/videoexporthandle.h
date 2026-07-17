// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_EXPORT_VIDEOEXPORTHANDLE_H
#define LIBCLIENT_EXPORT_VIDEOEXPORTHANDLE_H

// This acquire/release mechanism is used to keep the Windows Media
// Foundation libraries loaded. Call them when starting and finishing a
// video export thing so that e.g. checking video formats doesn't load and
// unload the library every time. It's not terrible if it happens, but it's
// better to avoid it.
#ifdef DP_WINDOWS_VIDEO_ENCODER
extern "C" {
#	include <dpimpex/windows_video_encoder.h>
}
#	include <QtGlobal>

class VideoExportHandle final {
	Q_DISABLE_COPY_MOVE(VideoExportHandle)
public:
	VideoExportHandle()
		: m_wmf(DP_windows_media_foundation_acquire())
	{
	}

	~VideoExportHandle() { DP_windows_media_foundation_release(m_wmf); }

private:
	DP_WindowsMediaFoundation *m_wmf;
};

#	define DP_DECLARE_VIDEO_EXPORT_HANDLE(NAME) const VideoExportHandle NAME;
#else
#	define DP_DECLARE_VIDEO_EXPORT_HANDLE(NAME) /* nothing */
#endif

#endif
