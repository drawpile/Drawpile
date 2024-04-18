/*
 * SPDX-FileCopyrightText: 2013-2020 Mattia Basaglia
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "QtColorWidgets/bound_color_selector.hpp"

namespace color_widgets {

BoundColorSelector::BoundColorSelector(QColor* reference, QWidget *parent) :
    ColorSelector(parent), ref(reference)
{
    setColor(*reference);
    connect(this,&ColorPreview::colorChanged,this, &BoundColorSelector::update_reference);
}

void BoundColorSelector::update_reference(QColor c)
{
    *ref = c;
}

} // namespace color_widgets
