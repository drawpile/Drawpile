/**

@author Mattia Basaglia

@section License

    Copyright (C) 2013-2015 Mattia Basaglia
    Copyright (C) 2014 Calle Laakkonen

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

#include "color_dialog.hpp"
#include "ui_color_dialog.h"

#include <QDropEvent>
#include <QDragEnterEvent>
#include <QDesktopWidget>
#include <QMimeData>
#include <QPushButton>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QScreen>
#endif

class Color_Dialog::Private
{
public:
    Ui_Color_Dialog ui;
    Button_Mode button_mode;
    bool pick_from_screen;
    bool alpha_enabled;

    Private() : pick_from_screen(false), alpha_enabled(true)
    {}

};

Color_Dialog::Color_Dialog(QWidget *parent, Qt::WindowFlags f) :
    QDialog(parent, f), p(new Private)
{
    p->ui.setupUi(this);

    setAcceptDrops(true);

    // Add "pick color" button
    QPushButton *pickButton = p->ui.buttonBox->addButton(tr("Pick"), QDialogButtonBox::ActionRole);
    pickButton->setIcon(QIcon::fromTheme("color-picker"));

    setButtonMode(OkApplyCancel);

    connect(p->ui.wheel,SIGNAL(displayFlagsChanged(Color_Wheel::Display_Flags)),SIGNAL(wheelFlagsChanged(Color_Wheel::Display_Flags)));
}

QSize Color_Dialog::sizeHint() const
{
    return QSize(400,0);
}

Color_Wheel::Display_Flags Color_Dialog::wheelFlags() const
{
    return p->ui.wheel->displayFlags();
}

QColor Color_Dialog::color() const
{
    QColor col = p->ui.wheel->color();
    if(p->alpha_enabled)
        col.setAlpha(p->ui.slide_alpha->value());
    return col;
}

void Color_Dialog::setColor(const QColor &c)
{
    p->ui.preview->setComparisonColor(c);
    setColorInternal(c);
}

void Color_Dialog::setColorInternal(const QColor &c)
{
    // Note. The difference between this method and setColor, is that setColor
    // sets the official starting color of the dialog, while this is used to update
    // the current color which might not be the final selected color.
    p->ui.wheel->setColor(c);
    p->ui.slide_alpha->setValue(c.alpha());
    update_widgets();
}

void Color_Dialog::showColor(const QColor &c)
{
    setColor(c);
    show();
}

void Color_Dialog::setWheelFlags(Color_Wheel::Display_Flags flags)
{
    p->ui.wheel->setDisplayFlags(flags);
}

void Color_Dialog::setPreviewDisplayMode(Color_Preview::Display_Mode mode)
{
    p->ui.preview->setDisplayMode(mode);
}

Color_Preview::Display_Mode Color_Dialog::previewDisplayMode() const
{
    return p->ui.preview->displayMode();
}

void Color_Dialog::setAlphaEnabled(bool a)
{
    p->alpha_enabled = a;

    p->ui.line_alpha->setVisible(a);
    p->ui.label_alpha->setVisible(a);
    p->ui.slide_alpha->setVisible(a);
    p->ui.spin_alpha->setVisible(a);
}

bool Color_Dialog::alphaEnabled() const
{
    return p->alpha_enabled;
}

void Color_Dialog::setButtonMode(Button_Mode mode)
{
    p->button_mode = mode;
    QDialogButtonBox::StandardButtons btns;
    switch(mode) {
        case OkCancel: btns = QDialogButtonBox::Ok | QDialogButtonBox::Cancel; break;
        case OkApplyCancel: btns = QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply | QDialogButtonBox::Reset; break;
        case Close: btns = QDialogButtonBox::Close;
    }
    p->ui.buttonBox->setStandardButtons(btns);
}

Color_Dialog::Button_Mode Color_Dialog::buttonMode() const
{
    return p->button_mode;
}

void Color_Dialog::update_widgets()
{
    bool blocked = signalsBlocked();
    blockSignals(true);
    foreach(QWidget* w, findChildren<QWidget*>())
        w->blockSignals(true);

    QColor col = color();

    p->ui.slide_red->setValue(col.red());
    p->ui.spin_red->setValue(p->ui.slide_red->value());
    p->ui.slide_red->setFirstColor(QColor(0,col.green(),col.blue()));
    p->ui.slide_red->setLastColor(QColor(255,col.green(),col.blue()));

    p->ui.slide_green->setValue(col.green());
    p->ui.spin_green->setValue(p->ui.slide_green->value());
    p->ui.slide_green->setFirstColor(QColor(col.red(),0,col.blue()));
    p->ui.slide_green->setLastColor(QColor(col.red(),255,col.blue()));

    p->ui.slide_blue->setValue(col.blue());
    p->ui.spin_blue->setValue(p->ui.slide_blue->value());
    p->ui.slide_blue->setFirstColor(QColor(col.red(),col.green(),0));
    p->ui.slide_blue->setLastColor(QColor(col.red(),col.green(),255));

    p->ui.slide_hue->setValue(qRound(p->ui.wheel->hue()*360.0));
    p->ui.slide_hue->setColorSaturation(p->ui.wheel->saturation());
    p->ui.slide_hue->setColorValue(p->ui.wheel->value());
    p->ui.spin_hue->setValue(p->ui.slide_hue->value());

    p->ui.slide_saturation->setValue(qRound(p->ui.wheel->saturation()*255.0));
    p->ui.spin_saturation->setValue(p->ui.slide_saturation->value());
    p->ui.slide_saturation->setFirstColor(QColor::fromHsvF(p->ui.wheel->hue(),0,p->ui.wheel->value()));
    p->ui.slide_saturation->setLastColor(QColor::fromHsvF(p->ui.wheel->hue(),1,p->ui.wheel->value()));

    p->ui.slide_value->setValue(qRound(p->ui.wheel->value()*255.0));
    p->ui.spin_value->setValue(p->ui.slide_value->value());
    p->ui.slide_value->setFirstColor(QColor::fromHsvF(p->ui.wheel->hue(), p->ui.wheel->saturation(),0));
    p->ui.slide_value->setLastColor(QColor::fromHsvF(p->ui.wheel->hue(), p->ui.wheel->saturation(),1));


    QColor apha_color = col;
    apha_color.setAlpha(0);
    p->ui.slide_alpha->setFirstColor(apha_color);
    apha_color.setAlpha(255);
    p->ui.slide_alpha->setLastColor(apha_color);
    p->ui.spin_alpha->setValue(p->ui.slide_alpha->value());


    p->ui.edit_hex->setText(col.name());

    p->ui.preview->setColor(col);

    blockSignals(blocked);
    foreach(QWidget* w, findChildren<QWidget*>())
        w->blockSignals(false);

    emit colorChanged(col);
}

void Color_Dialog::set_hsv()
{
    if ( !signalsBlocked() )
    {
        p->ui.wheel->setColor(QColor::fromHsv(
                p->ui.slide_hue->value(),
                p->ui.slide_saturation->value(),
                p->ui.slide_value->value()
            ));
        update_widgets();
    }
}

void Color_Dialog::set_rgb()
{
    if ( !signalsBlocked() )
    {
        QColor col(
                p->ui.slide_red->value(),
                p->ui.slide_green->value(),
                p->ui.slide_blue->value()
            );
        if (col.saturation() == 0)
            col = QColor::fromHsv(p->ui.slide_hue->value(), 0, col.value());
        p->ui.wheel->setColor(col);
        update_widgets();
    }
}


void Color_Dialog::on_edit_hex_editingFinished()
{
    update_hex();

}

void Color_Dialog::on_edit_hex_textEdited(const QString &arg1)
{
    int cursor = p->ui.edit_hex->cursorPosition();
    update_hex();
    //edit_hex->blockSignals(true);
    p->ui.edit_hex->setText(arg1);
    //edit_hex->blockSignals(false);
    p->ui.edit_hex->setCursorPosition(cursor);
}

void Color_Dialog::update_hex()
{
    QString xs = p->ui.edit_hex->text().trimmed();
    xs.remove('#');

    if ( xs.isEmpty() )
        return;

    if ( xs.indexOf(QRegExp("^[0-9a-fA-f]+$")) == -1 )
    {
        QColor c(xs);
        if ( c.isValid() )
        {
            setColorInternal(c);
            return;
        }
    }

    if ( xs.size() == 3 )
    {
        p->ui.slide_red->setValue(QString(2,xs[0]).toInt(0,16));
        p->ui.slide_green->setValue(QString(2,xs[1]).toInt(0,16));
        p->ui.slide_blue->setValue(QString(2,xs[2]).toInt(0,16));
    }
    else
    {
        if ( xs.size() < 6 )
        {
            xs += QString(6-xs.size(),'0');
        }
        p->ui.slide_red->setValue(xs.mid(0,2).toInt(0,16));
        p->ui.slide_green->setValue(xs.mid(2,2).toInt(0,16));
        p->ui.slide_blue->setValue(xs.mid(4,2).toInt(0,16));

        if ( xs.size() == 8 )
            p->ui.slide_alpha->setValue(xs.mid(6,2).toInt(0,16));
    }

    set_rgb();
}

void Color_Dialog::on_buttonBox_clicked(QAbstractButton *btn)
{
    QDialogButtonBox::ButtonRole role = p->ui.buttonBox->buttonRole(btn);

    switch(role) {
    case QDialogButtonBox::AcceptRole:
    case QDialogButtonBox::ApplyRole:
        // Explicitly select the color
        p->ui.preview->setComparisonColor(color());
        emit colorSelected(color());
        break;

    case QDialogButtonBox::ActionRole:
        // Currently, the only action button is the "pick color" button
        grabMouse(Qt::CrossCursor);
        p->pick_from_screen = true;
        break;

    case QDialogButtonBox::ResetRole:
        // Restore old color
        setColorInternal(p->ui.preview->comparisonColor());
        break;

    default: break;
    }
}

void Color_Dialog::dragEnterEvent(QDragEnterEvent *event)
{
    if ( event->mimeData()->hasColor() ||
         ( event->mimeData()->hasText() && QColor(event->mimeData()->text()).isValid() ) )
        event->acceptProposedAction();
}


void Color_Dialog::dropEvent(QDropEvent *event)
{
    if ( event->mimeData()->hasColor() )
    {
        setColorInternal(event->mimeData()->colorData().value<QColor>());
        event->accept();
    }
    else if ( event->mimeData()->hasText() )
    {
        QColor col(event->mimeData()->text());
        if ( col.isValid() )
        {
            setColorInternal(col);
            event->accept();
        }
    }
}

namespace {
QColor get_screen_color(const QPoint &global_pos)
{
#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    WId id = QApplication::desktop()->winId();
    QImage img = QPixmap::grabWindow(id, global_pos.x(), global_pos.y(), 1, 1).toImage();
#else
    int screenNum = QApplication::desktop()->screenNumber(global_pos);
    QScreen *screen = QApplication::screens().at(screenNum);

    WId wid = QApplication::desktop()->winId();
    QImage img = screen->grabWindow(wid, global_pos.x(), global_pos.y(), 1, 1).toImage();
#endif

    return img.pixel(0,0);
}
}

void Color_Dialog::mouseReleaseEvent(QMouseEvent *event)
{
    if (p->pick_from_screen)
    {
        setColorInternal(get_screen_color(event->globalPos()));
        p->pick_from_screen = false;
        releaseMouse();
    }
}

void Color_Dialog::mouseMoveEvent(QMouseEvent *event)
{
    if (p->pick_from_screen)
    {
        setColorInternal(get_screen_color(event->globalPos()));
    }
}

