/*
 * SPDX-FileCopyrightText: 2013-2020 Mattia Basaglia
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "QtColorWidgets/color_names.hpp"
#include <QRegularExpression>

static QRegularExpression regex_hex_rgb (QStringLiteral("^#?(?:[[:xdigit:]]{3}|[[:xdigit:]]{6})$"));
static QRegularExpression regex_qcolor (QStringLiteral("^[[:alpha:]]+$"));
static QRegularExpression regex_func_rgb (QStringLiteral(R"(^rgb\s*\(\s*([0-9]+)\s*,\s*([0-9]+)\s*,\s*([0-9]+)\s*\)$)"));
static QRegularExpression regex_hex_rgba (QStringLiteral("^#?([[:xdigit:]]{2})([[:xdigit:]]{2})([[:xdigit:]]{2})([[:xdigit:]]{2})$"));
static QRegularExpression regex_func_rgba (QStringLiteral(R"(^rgba?\s*\(\s*([0-9]+)\s*,\s*([0-9]+)\s*,\s*([0-9]+)\s*,\s*([0-9]+)\s*\)$)"));

namespace color_widgets {


QString stringFromColor(const QColor& color, bool alpha)
{
    if ( !alpha || color.alpha() == 255 )
        return color.name();
    return color.name()+QStringLiteral("%1").arg(color.alpha(), 2, 16, QLatin1Char('0'));
}

QColor colorFromString(const QString& string, bool alpha)
{
    QString xs = string.trimmed();
    QRegularExpressionMatch match;

    match = regex_hex_rgb.match(xs);
    if ( match.hasMatch() )
    {
        if ( xs.startsWith('#') )
            return QColor(xs);
        else
            return QColor(QStringLiteral("#") + xs);
    }

    match = regex_qcolor.match(xs);
    if ( match.hasMatch() )
    {
        return QColor(xs);
    }

    match = regex_func_rgb.match(xs);
    if ( match.hasMatch() )
    {
        return QColor(
            match.captured(1).toInt(),
            match.captured(2).toInt(),
            match.captured(3).toInt()
        );
    }

    if ( alpha )
    {
        match = regex_hex_rgba.match(xs);
        if ( match.hasMatch() )
        {
            return QColor(
                match.captured(1).toInt(nullptr,16),
                match.captured(2).toInt(nullptr,16),
                match.captured(3).toInt(nullptr,16),
                match.captured(4).toInt(nullptr,16)
            );
        }

        match = regex_func_rgba.match(xs);
        if ( match.hasMatch() )
        {
            return QColor(
                match.captured(1).toInt(),
                match.captured(2).toInt(),
                match.captured(3).toInt(),
                match.captured(4).toInt()
            );
        }
    }

    return QColor();
}

} // namespace color_widgets
