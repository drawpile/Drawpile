/**

@author Mattia Basaglia

@section License

    Copyright (C) 2015 Mattia Basaglia

    This software is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This software is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Color Widgets.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "color_line_edit.hpp"

#include <QRegularExpression>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QApplication>
#include <QPainter>
#include <QStyleOptionFrame>

#include "color_utils.hpp"

namespace color_widgets {

static QRegularExpression regex_qcolor ("^(?:(?:#[[:xdigit:]]{3})|(?:#[[:xdigit:]]{6})|(?:[[:alpha:]]+))$");
static QRegularExpression regex_func_rgb (R"(^rgb\s*\(\s*([0-9]+)\s*,\s*([0-9]+)\s*,\s*([0-9]+)\s*\)$)");
static QRegularExpression regex_hex_rgba ("^#[[:xdigit:]]{8}$");
static QRegularExpression regex_func_rgba (R"(^rgba?\s*\(\s*([0-9]+)\s*,\s*([0-9]+)\s*,\s*([0-9]+)\s*,\s*([0-9]+)\s*\)$)");

class ColorLineEdit::Private
{
public:
    QColor color;
    bool show_alpha = false;
    bool preview_color = false;
    QBrush background;

    QString stringFromColor(const QColor& c)
    {
        if ( !show_alpha || c.alpha() == 255 )
            return c.name();
        return c.name()+QString::number(c.alpha(),16);
    }

    QColor colorFromString(const QString& s)
    {
        QString xs = s.trimmed();
        QRegularExpressionMatch match;

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

        if ( show_alpha )
        {
            match = regex_hex_rgba.match(xs);
            if ( match.hasMatch() )
            {
                return QColor(
                    xs.mid(1,2).toInt(nullptr,16),
                    xs.mid(3,2).toInt(nullptr,16),
                    xs.mid(5,2).toInt(nullptr,16),
                    xs.mid(7,2).toInt(nullptr,16)
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

    bool customAlpha()
    {
        return preview_color && show_alpha && color.alpha() < 255;
    }

    void setPalette(const QColor& color, ColorLineEdit* parent)
    {
        if ( preview_color )
        {
            QPalette pal = parent->palette();

            if ( customAlpha() )
                pal.setColor(QPalette::Base, Qt::transparent);
            else
                pal.setColor(QPalette::Base, color.rgb());
            pal.setColor(QPalette::Text,
                detail::color_lumaF(color) > 0.5 || color.alphaF() < 0.2 ? Qt::black : Qt::white);
            parent->setPalette(pal);
        }
    }
};

ColorLineEdit::ColorLineEdit(QWidget* parent)
    : QLineEdit(parent), p(new Private)
{
    p->background.setTexture(QPixmap(QLatin1String(":/color_widgets/alphaback.png")));
    setColor(Qt::white);
    /// \todo determine if having this connection might be useful
    /*connect(this, &QLineEdit::textChanged, [this](const QString& text){
        QColor color = p->colorFromString(text);
        if ( color.isValid() )
            emit colorChanged(color);
    });*/
    connect(this, &QLineEdit::textEdited, [this](const QString& text){
        QColor color = p->colorFromString(text);
        if ( color.isValid() )
        {
            p->color = color;
            p->setPalette(color, this);
            emit colorEdited(color);
            emit colorChanged(color);
        }
    });
    connect(this, &QLineEdit::editingFinished, [this](){
        QColor color = p->colorFromString(text());
        if ( color.isValid() )
        {
            p->color = color;
            emit colorEditingFinished(color);
            emit colorChanged(color);
        }
        else
        {
            setText(p->stringFromColor(p->color));
            emit colorEditingFinished(p->color);
            emit colorChanged(color);
        }
        p->setPalette(p->color, this);
    });
}

ColorLineEdit::~ColorLineEdit()
{
    delete p;
}

QColor ColorLineEdit::color() const
{
    return p->color;
}

void ColorLineEdit::setColor(const QColor& color)
{
    if ( color != p->color )
    {
        p->color = color;
        p->setPalette(p->color, this);
        setText(p->stringFromColor(p->color));
        emit colorChanged(p->color);
    }
}

void ColorLineEdit::setShowAlpha(bool showAlpha)
{
    if ( p->show_alpha != showAlpha )
    {
        p->show_alpha = showAlpha;
        p->setPalette(p->color, this);
        setText(p->stringFromColor(p->color));
        emit showAlphaChanged(p->show_alpha);
    }
}

bool ColorLineEdit::showAlpha() const
{
    return p->show_alpha;
}

void ColorLineEdit::dragEnterEvent(QDragEnterEvent *event)
{
    if ( isReadOnly() )
        return;

    if ( event->mimeData()->hasColor() ||
         ( event->mimeData()->hasText() && p->colorFromString(event->mimeData()->text()).isValid() ) )
        event->acceptProposedAction();
}


void ColorLineEdit::dropEvent(QDropEvent *event)
{
    if ( isReadOnly() )
        return;
    
    if ( event->mimeData()->hasColor() )
    {
        setColor(event->mimeData()->colorData().value<QColor>());
        event->accept();
    }
    else if ( event->mimeData()->hasText() )
    {
        QColor col =  p->colorFromString(event->mimeData()->text());
        if ( col.isValid() )
        {
            setColor(col);
            event->accept();
        }
    }
}

bool ColorLineEdit::previewColor() const
{
    return p->preview_color;
}

void ColorLineEdit::setPreviewColor(bool previewColor)
{
    if ( previewColor != p->preview_color )
    {
        p->preview_color = previewColor;

        if ( p->preview_color )
            p->setPalette(p->color, this);
        else
            setPalette(QApplication::palette());

        emit previewColorChanged(p->preview_color);
    }
}

void ColorLineEdit::paintEvent(QPaintEvent* event)
{
    if ( p->customAlpha() )
    {
        QPainter painter(this);
        QStyleOptionFrame panel;
        initStyleOption(&panel);
        QRect r = style()->subElementRect(QStyle::SE_LineEditContents, &panel, nullptr);
        painter.fillRect(r, p->background);
        painter.fillRect(r, p->color);
    }

    QLineEdit::paintEvent(event);
}

} // namespace color_widgets
