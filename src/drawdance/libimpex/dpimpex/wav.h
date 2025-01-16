// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_WAV_H
#define DPIMPEX_WAV_H
#include <dpcommon/common.h>

// Read a WAV file, change its volume in percent. This function exists so that
// we can use it with the Win32 function PlaySound in soundplayer_win32.cpp,
// which doesn't support setting a volume.
unsigned char *DP_wav_read_from_path(const char *path, int volume,
                                     size_t *out_length);

#endif
