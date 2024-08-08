// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_UTF16BE_H
#define DPIMPEX_UTF16BE_H
#include <dpcommon/common.h>

// Callback gives the size in bytes and is not guaranteed to be 16-bit aligned!
bool DP_utf8_to_utf16be(const char *s,
                        bool (*set_utf16be)(void *, const char *, size_t),
                        void *user);

bool DP_utf16be_to_utf8(const uint16_t *s,
                        bool (*set_utf8)(void *, const char *, size_t),
                        void *user);

#endif
