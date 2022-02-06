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
#include "dpmsg_test.h"
#include "dpcommon/output.h"
#include "dpcommon_test.h"
#include "dpmsg/binary_writer.h"
#include "dpmsg/message.h"
#include "dpmsg/text_writer.h"
#include <dpmsg/binary_reader.h>


static void destroy_binary_reader(void *value)
{
    DP_binary_reader_free(value);
}

void push_binary_reader(void **state, DP_BinaryReader *value, DP_Input *input)
{
    destructor_remove(state, input);
    destructor_push(state, value, destroy_binary_reader);
}

static void destroy_binary_writer(void *value)
{
    DP_binary_writer_free(value);
}

void push_binary_writer(void **state, DP_BinaryWriter *value, DP_Output *output)
{
    destructor_remove(state, output);
    destructor_push(state, value, destroy_binary_writer);
}

static void destroy_message(void *value)
{
    DP_message_decref(value);
}

void push_message(void **state, DP_Message *value)
{
    destructor_push(state, value, destroy_message);
}

static void destroy_text_writer(void *value)
{
    DP_text_writer_free(value);
}

void push_text_writer(void **state, DP_TextWriter *value, DP_Output *output)
{
    destructor_remove(state, output);
    destructor_push(state, value, destroy_text_writer);
}
