#ifndef MYPAINTBRUSHSETTINGS_H
#define MYPAINTBRUSHSETTINGS_H

/* libmypaint - The MyPaint Brush Library
 * Copyright (C) 2012 Jon Nordby <jononor@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "mypaint-config.h"
#include "mypaint-glib-compat.h"
#include "mypaint-brush-settings-gen.h"

G_BEGIN_DECLS

/*!
 * @brief Holds information about a single ::MyPaintBrushSetting
 */
typedef struct {
    /*! Canonical name - uniquely identifies the setting */
    const gchar *cname;
    /*! Display name - descriptive name of the setting
     *
     * @sa mypaint_brush_setting_info_get_name
     */
    const gchar *name;
    /*! Indicates whether the setting supports dynamics or not */
    gboolean constant;
    /*! Mininmum base value */
    float min;
    /*! Default base value */
    float def; // default
    /*! Maximum base value */
    float max;
    /*! Tooltip - description of the setting
     *
     * @sa mypaint_brush_setting_info_get_tooltip
     */
    const gchar *tooltip;
} MyPaintBrushSettingInfo;

/*!
 * @brief Get a pointer to the constant info struct for the given setting id.
 *
 * @param id The ::MyPaintBrushSetting whose information will be returned.
 *
 * @memberof MyPaintBrushSettingInfo
 */
const MyPaintBrushSettingInfo *
mypaint_brush_setting_info(MyPaintBrushSetting id);

/*!
 * @brief Get the translated display name for the given setting.
 *
 * @param self pointer to the settings info struct whose translated name will be returned.
 * @returns the translated display name, or the original display name if no translation
 * exists for the active locale.
 * @memberof MyPaintBrushSettingInfo
 */
const gchar *
mypaint_brush_setting_info_get_name(const MyPaintBrushSettingInfo *self);

/*!
 * @brief Get the translated tooltip for the given setting
 *
 * @param self settings info object whose translated tooltip will be returned.
 * @returns the translated tooltip, or the original tooltip if no translation
 * exists for the active locale.
 * @memberof MyPaintBrushSettingInfo
 */
const gchar *
mypaint_brush_setting_info_get_tooltip(const MyPaintBrushSettingInfo *self);

/*!
 * @brief Get the ::MyPaintBrushSetting id matching the given canonical name.
 *
 * If the given char sequence is not a canonical name of a setting, -1 is returned.
 *
 * @param cname the canonical name for the setting id to retrieve
 * @returns The id whose corresponding \ref MyPaintBrushSettingInfo::cname
 * matches __cname__, or -1 if __cname__ is not a valid canonical name.
 *
 * @memberof MyPaintBrushSettingInfo
 */
MyPaintBrushSetting
mypaint_brush_setting_from_cname(const char *cname);

/*!
 * @brief Holds information about a single @ref MyPaintBrushInput
 *
 * Like MyPaintBrushSettingInfo, these structs hold information
 * about canonical names, display names and tooltips. The remaining
 * data should be used to limit how the inputs are mapped to settings.
 *
 * The __min__ and __max__ values in MyPaintBrushSettingInfo define limits
 * on the range that an input should be mapped _to_ for a setting.
 * The __hard/soft_min/max__ values in this struct define limits on the
 * input range for mappings _from_ an input.
 */
typedef struct {
    /*! Canonical name - uniquely identifies the input */
    const gchar *cname;
    /*!
     * Hard lower limit of the input's range
     *
     * A value of -FLT_MAX should be interpreted as "undefined", and
     * a constant minimum should be used instead (MyPaint uses -20).
     */
    float hard_min;
    /*! Default lower limit of the input's range */
    float soft_min;
    /*!
     * Expected normal value for the input - only used as a reference
     * when creating mappings.
     */
    float normal;
    /*! Default upper limit of the input's range */
    float soft_max;
    /*!
     * Hard upper limit of the input's range
     *
     * A value of FLT_MAX should be interpreted as "undefined", and
     * a constant maximum should be used instead (MyPaint uses 20).
     */
    float hard_max;
    /*! Display name - descriptive name of the input
     *
     * @sa mypaint_brush_input_info_get_name
     */
    const gchar *name;
    /*! Tooltip - brief description of the input.
     *
     * @sa mypaint_brush_input_info_get_tooltip
     */
    const gchar *tooltip;
} MyPaintBrushInputInfo;

/*!
 * @brief Get a pointer to the constant info struct for the given input id.
 *
 * @param id The ::MyPaintBrushInput whose information will be returned.
 *
 * @memberof MyPaintBrushInputInfo
 */
const MyPaintBrushInputInfo *
mypaint_brush_input_info(MyPaintBrushInput id);

/*!
 * @brief Get the translated display name for the given input.
 *
 * @param self pointer to the input info struct whose translated name will be returned.
 * @returns the translated display name, or the original display name if no translation
 * exists for the active locale.
 * @memberof MyPaintBrushInputInfo
 */
const gchar *
mypaint_brush_input_info_get_name(const MyPaintBrushInputInfo *self);
/*!
 * @brief Get the translated display name for the given input.
 *
 * @param self pointer to the input info struct whose translated name will be returned.
 * @returns the translated tooltip, or the original tooltip if no translation
 * exists for the active locale.
 * @memberof MyPaintBrushInputInfo
 */
const gchar *
mypaint_brush_input_info_get_tooltip(const MyPaintBrushInputInfo *self);

/*!
 * @brief Get the ::MyPaintBrushInput id matching the given canonical name.
 *
 * If the given input is not a canonical name of an input, -1 is returned.
 *
 * @param cname the canonical name for the input id to retrieve
 * @returns The id whose corresponding \ref MyPaintBrushInputInfo::cname
 * matches __cname__, or -1 if __cname__ is not a valid canonical name.
 *
 * @memberof MyPaintBrushInputInfo
 */
MyPaintBrushInput
mypaint_brush_input_from_cname(const char *cname);

G_END_DECLS

#endif // MYPAINTBRUSHSETTINGS_H
