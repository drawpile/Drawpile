// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_EXPORT_ANIMATIONFORMAT_H
#define LIBCLIENT_EXPORT_ANIMATIONFORMAT_H

enum class AnimationFormat { Frames, Gif, Webp, Mp4, Webm };

bool isAnimationFormatSupported(AnimationFormat format);

#endif
