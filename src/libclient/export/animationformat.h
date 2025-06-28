// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_EXPORT_ANIMATIONFORMAT_H
#define LIBCLIENT_EXPORT_ANIMATIONFORMAT_H

enum class AnimationFormat {
	Frames,
	Zip,
	Gif,
	Webp,
	Mp4Vp9,
	WebmVp8,
};

bool isAnimationFormatSupported(AnimationFormat format);

#endif
