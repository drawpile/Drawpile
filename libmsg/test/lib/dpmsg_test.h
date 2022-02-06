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
#ifndef DPMSG_TEST_H
#define DPMSG_TEST_H
#include <dpcommon_test.h> // IWYU pragma: export

typedef struct DP_BinaryReader DP_BinaryReader;
typedef struct DP_BinaryWriter DP_BinaryWriter;
typedef struct DP_Message DP_Message;
typedef struct DP_TextWriter DP_TextWriter;


void push_binary_reader(void **state, DP_BinaryReader *value, DP_Input *input);

void push_binary_writer(void **state, DP_BinaryWriter *value,
                        DP_Output *output);

void push_message(void **state, DP_Message *value);

void push_text_writer(void **state, DP_TextWriter *value, DP_Output *output);


#endif
