// SPDX-License-Identifier: MIT
#include "text.h"
#include <dpcommon/atomic.h>
#include <dpcommon/common.h>


struct DP_Text {
    DP_Atomic refcount;
    size_t length;
    char string[];
};

DP_Text *DP_text_new(const char *string_or_null, size_t length)
{
    if (string_or_null && length != 0) {
        DP_Text *text = DP_malloc(DP_FLEX_SIZEOF(DP_Text, string, length + 1));
        DP_atomic_set(&text->refcount, 1);
        text->length = length;
        memcpy(text->string, string_or_null, length);
        text->string[length] = '\0';
        return text;
    }
    else {
        return NULL;
    }
}

DP_Text *DP_text_new_nolen(const char *string_or_null)
{
    return DP_text_new(string_or_null,
                       string_or_null ? strlen(string_or_null) : 0);
}

DP_Text *DP_text_incref_nullable(DP_Text *text_or_null)
{
    if (text_or_null) {
        DP_ASSERT(DP_atomic_get(&text_or_null->refcount) > 0);
        DP_atomic_inc(&text_or_null->refcount);
        return text_or_null;
    }
    else {
        return NULL;
    }
}

void DP_text_decref_nullable(DP_Text *text_or_null)
{
    if (text_or_null) {
        DP_ASSERT(DP_atomic_get(&text_or_null->refcount) > 0);
        if (DP_atomic_dec(&text_or_null->refcount)) {
            DP_free(text_or_null);
        }
    }
}

int DP_text_refcount(DP_Text *text_or_null)
{
    if (text_or_null) {
        DP_ASSERT(DP_atomic_get(&text_or_null->refcount) > 0);
        return DP_atomic_get(&text_or_null->refcount);
    }
    else {
        return 0;
    }
}

const char *DP_text_string(DP_Text *text_or_null, size_t *out_length)
{
    const char *string;
    size_t length;
    if (text_or_null) {
        DP_ASSERT(DP_atomic_get(&text_or_null->refcount) > 0);
        string = text_or_null->string;
        length = text_or_null->length;
    }
    else {
        string = "";
        length = 0;
    }

    if (out_length) {
        *out_length = length;
    }
    return string;
}

bool DP_text_equal(DP_Text *a_or_null, DP_Text *b_or_null)
{
    if (a_or_null == b_or_null) {
        return true;
    }
    else {
        size_t a_length, b_length;
        const char *a = DP_text_string(a_or_null, &a_length);
        const char *b = DP_text_string(b_or_null, &b_length);
        return a_length == b_length && memcmp(a, b, a_length) == 0;
    }
}
