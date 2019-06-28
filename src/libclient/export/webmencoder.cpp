/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "webmencoder.h"

#include <QImage>
#include <QFile>

WebmEncoder::WebmEncoder(const QString &filename, QObject *parent)
	: QObject(parent), m_initialized(false)
{
	m_writer.setFilename(filename);
}

WebmEncoder::~WebmEncoder()
{
	vpx_img_free(&m_rawFrame);
	vpx_codec_destroy(&m_codec);
}

void WebmEncoder::open()
{
	if(!m_writer.open()) {
		emit encoderError(m_writer.errorString());
		return;
	}

	m_segment.Init(&m_writer);
	m_timecode = 0;

	emit encoderReady();
}

void WebmEncoder::start(int width, int height, int fps)
{
	m_videoTrack = m_segment.AddVideoTrack(width, height, 1);
	static_cast<mkvmuxer::VideoTrack*>(m_segment.GetTrackByNumber(m_videoTrack))->set_codec_id(mkvmuxer::Tracks::kVp9CodecId);

	if(!vpx_img_alloc(&m_rawFrame, VPX_IMG_FMT_I444, width, height, 1)) {
		emit encoderError("Failed to allocate frame buffer!");
		return;
	}

	const vpx_codec_iface_t *codecInterface = vpx_codec_vp9_cx();

	vpx_codec_enc_cfg_t cfg;
	if(vpx_codec_enc_config_default(codecInterface, &cfg, 0)) {
		emit encoderError("Failed to get default codec config!");
		return;
	}

	cfg.g_w = width;
	cfg.g_h = height;
	cfg.g_timebase = {1, fps};
	cfg.rc_target_bitrate = 200;
	cfg.g_error_resilient = VPX_ERROR_RESILIENT_DEFAULT;
	cfg.g_profile = 1; // Profile 1 needed for 4:4:4 format
	m_fps = fps;

	if (vpx_codec_enc_init(&m_codec, codecInterface, &cfg, 0)) {
		emit encoderError("Failed to initialize encoder!");
		return;
	}

	// Set sRGB color space
	if (vpx_codec_control(&m_codec, VP9E_SET_COLOR_SPACE, 7)) {
		qWarning("VPX error: %s (%s)",
				vpx_codec_error(&m_codec),
				vpx_codec_error_detail(&m_codec));
		emit encoderError("Failed to set encoder color space!");
		return;
	}

	m_initialized = true;
}

void WebmEncoder::writeFrame(const QImage &image, int repeat)
{
	if(!m_initialized) {
		qWarning("WebmEncoder::writeFrame called but encoder is not initialized!");
		return;
	}

	const unsigned int w=image.width();
	const unsigned int h=image.height();
	Q_ASSERT(w == m_rawFrame.w);
	Q_ASSERT(h == m_rawFrame.h);
	Q_ASSERT(image.depth() == 32);
	Q_ASSERT(repeat>0);

	// Color space is actually sRGB
	uchar *yplane = m_rawFrame.planes[VPX_PLANE_Y];
	uchar *uplane = m_rawFrame.planes[VPX_PLANE_U];
	uchar *vplane = m_rawFrame.planes[VPX_PLANE_V];
	for(unsigned int y=0;y<h;++y) {
		const uchar *src = image.constScanLine(y);
		for(unsigned int x=0;x<w;++x, src+=4) {
			vplane[x] = src[2]; // red
			yplane[x] = src[1]; // green
			uplane[x] = src[0]; // blue
		}

		yplane += m_rawFrame.stride[VPX_PLANE_Y];
		uplane += m_rawFrame.stride[VPX_PLANE_U];
		vplane += m_rawFrame.stride[VPX_PLANE_V];
	}

	// Enqueue frame for encoding
	const uint64_t duration = uint64_t(repeat) * 1000000000 / m_fps;
	const vpx_codec_err_t res = vpx_codec_encode(
			&m_codec,
			&m_rawFrame,
			m_timecode,
			duration,
			0,
			VPX_DL_GOOD_QUALITY
			);

	if (res != VPX_CODEC_OK) {
		qWarning("VPX error: %s (%s)",
				vpx_codec_error(&m_codec),
				vpx_codec_error_detail(&m_codec));
		emit encoderError("Failed to encode frame!");
		return;
	}

	m_timecode += duration;

	writeFrames();

	emit encoderReady();
}

bool WebmEncoder::writeFrames()
{
	const vpx_codec_cx_pkt_t *pkt = nullptr;
	vpx_codec_iter_t iter = nullptr;

	bool gotPkts = false;
	while((pkt = vpx_codec_get_cx_data(&m_codec, &iter))) {
		if(pkt->kind == VPX_CODEC_CX_FRAME_PKT) {
			gotPkts = true;
			if(!m_segment.AddFrame(
					static_cast<uint8_t*>(pkt->data.frame.buf),
					pkt->data.frame.sz,
					m_videoTrack,
					pkt->data.frame.pts,
					pkt->data.frame.flags & VPX_FRAME_IS_KEY)
				) {
				emit encoderError("Error occurred while writing frame");
				return false;
			}
		}
	}
	return gotPkts;
}

void WebmEncoder::finish()
{
	if(!m_initialized) {
		qWarning("WebmEncoder::finish called but encoder is not initialized!");
		return;
	}

	// Flush the encoder
	do {
		const vpx_codec_err_t res =
			vpx_codec_encode(&m_codec, nullptr, 0, 0, 0, VPX_DL_GOOD_QUALITY);
		if (res != VPX_CODEC_OK) {
			emit encoderError("Error occurred while flushing encoder");
			return;
		}
	} while(writeFrames());


	// Close up
	m_segment.Finalize();
	m_writer.close();

	m_initialized = false;

	emit encoderFinished();
}

mkvmuxer::int32 QFileMkvWriter::Write(const void* buffer, mkvmuxer::uint32 length) {
	if(!m_file.isOpen())
		return -1;
	if(length==0)
		return 0;
	if(!buffer)
		return -1;

	const auto written = m_file.write(static_cast<const char*>(buffer), length);
	return written == length ? 0 : -1;
}
