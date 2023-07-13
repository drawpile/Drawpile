// SPDX-License-Identifier: MIT
#ifndef DPENGINE_TEXT_H
#define DPENGINE_TEXT_H
#include <dpcommon/common.h>


// Used for layer, track and key frame titles, as well as annotation contents.
typedef struct DP_Text DP_Text;

// Returns NULL for an empty/null input string, all functions handle null input.
DP_Text *DP_text_new(const char *string_or_null, size_t length);
DP_Text *DP_text_new_nolen(const char *string_or_null);

DP_Text *DP_text_incref_nullable(DP_Text *text_or_null);

void DP_text_decref_nullable(DP_Text *text_or_null);

int DP_text_refcount(DP_Text *text_or_null);

// Returns the empty string when given NULL.
const char *DP_text_string(DP_Text *text_or_null, size_t *out_length);

bool DP_text_equal(DP_Text *a_or_null, DP_Text *b_or_null);


#endif
