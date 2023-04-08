/*
 * Copyright (c) 2022 askmeaboutloom
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "uri_utils.h"
#include "client.h"
#include <dpcommon/common.h>
#include <ctype.h>
#include <uriparser/Uri.h>

bool DP_uri_parse(UriUriA *uri, const char *url)
{
    if (uriParseSingleUriA(uri, url, NULL) == URI_SUCCESS) {
        return true;
    }
    else {
        DP_error_set("Failed to parse URL");
        return false;
    }
}

size_t DP_uri_text_length(UriTextRangeA tr)
{
    if (tr.first && tr.afterLast) {
        ptrdiff_t diff = tr.afterLast - tr.first;
        if (diff > 0) {
            return (size_t)diff;
        }
    }
    return 0;
}

bool DP_uri_text_equal_ignore_case(UriTextRangeA tr, const char *s)
{
    size_t length = DP_uri_text_length(tr);
    if (length != strlen(s)) {
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (tolower(tr.first[i]) != tolower(s[i])) {
            return false;
        }
    }
    return true;
}

char *DP_uri_text_dup(UriTextRangeA tr)
{
    size_t length = DP_uri_text_length(tr);
    if (length > 0) {
        char *text = DP_malloc(length + 1);
        memcpy(text, tr.first, length);
        text[length] = '\0';
        return text;
    }
    return NULL;
}
