/*
 * SPDX-FileCopyrightText: 2013-2020 Mattia Basaglia
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef BOUND_COLOR_SELECTOR_HPP
#define BOUND_COLOR_SELECTOR_HPP

#include "color_selector.hpp"

namespace color_widgets {
/**
 * \brief A color selector bound to a color reference
 * \todo Maybe this can be removed
 */
class QCP_EXPORT BoundColorSelector : public ColorSelector
{
    Q_OBJECT
private:
    QColor* ref;
public:
    explicit BoundColorSelector(QColor* reference, QWidget *parent = nullptr);

private Q_SLOTS:
    void update_reference(QColor);
};
} // namespace color_widgets
#endif // BOUND_COLOR_SELECTOR_HPP
