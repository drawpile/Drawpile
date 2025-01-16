// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_UTILS_SOUNDPLAYER_H
#define DESKTOP_UTILS_SOUNDPLAYER_H
#include "libshared/util/qtcompat.h"
#include <QString>

// Wrapper around different sound playing implementations. Qt5 and Qt6 work
// differently here, so they have separate implementations. On Windows, we use a
// custom implementation that uses PlaySound directly, since Qt's implementation
// is known to crash on at least one Windows 7 system, so this is safer.
class SoundPlayer final {
	COMPAT_DISABLE_COPY_MOVE(SoundPlayer)
public:
	SoundPlayer();
	~SoundPlayer();

	void playSound(const QString &path, int volume);
	bool isPlaying() const;

private:
	struct Private;
	Private *d;
};

#endif
