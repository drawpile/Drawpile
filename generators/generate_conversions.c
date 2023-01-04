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
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

struct ConversionType {
    const char *type;
    const char *name;
};

const struct ConversionType conversion_types[] = {
    {"char", "char"},     {"unsigned char", "uchar"},
    {"short", "short"},   {"unsigned short", "ushort"},
    {"int", "int"},       {"unsigned int", "uint"},
    {"long", "long"},     {"unsigned long", "ulong"},
    {"int8_t", "int8"},   {"uint8_t", "uint8"},
    {"int16_t", "int16"}, {"uint16_t", "uint16"},
    {"int32_t", "int32"}, {"uint32_t", "uint32"},
    {"float", "float"},   {"double", "double"},
    {"size_t", "size"},
};

static void generate_conversion_function(FILE *fp, int i, int j)
{
    const struct ConversionType *from = &conversion_types[i];
    const struct ConversionType *to = &conversion_types[j];
    fprintf(fp, "DP_INLINE %s DP_%s_to_%s(%s x)\n", to->type, from->name,
            to->name, from->type);
    fprintf(fp, "{\n");
    fprintf(fp, "    return (%s)x;\n", to->type);
    fprintf(fp, "}\n");
    fprintf(fp, "\n");
}

static bool generate_h_file(const char *path)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        fprintf(stderr, "Can't open '%s': %s\n", path, strerror(errno));
        return false;
    }

    fprintf(fp, "// This is an auto-generated file, don't edit it directly.\n");
    fprintf(
        fp,
        "// Look for the generator in generators/generate_conversions.c.\n");
    fprintf(fp, "#ifndef DPCOMMON_CONVERSIONS_H\n");
    fprintf(fp, "#define DPCOMMON_CONVERSIONS_H\n");
    fprintf(fp, "#include \"common.h\"\n");
    fprintf(fp, "\n");

    int count = (int)(sizeof(conversion_types) / sizeof(conversion_types[0]));
    for (int i = 0; i < count; ++i) {
        for (int j = 0; j < count; ++j) {
            if (i != j) {
                generate_conversion_function(fp, i, j);
            }
        }
    }

    fprintf(fp, "#endif\n");

    if (fclose(fp) != 0) {
        fprintf(stderr, "Can't close '%s': %s\n", path, strerror(errno));
        return false;
    }

    return true;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s H_FILE_PATH\n",
                argc == 0 ? "generate_conversions" : argv[0]);
        return 2;
    }
    return generate_h_file(argv[1]) ? 0 : 1;
}
