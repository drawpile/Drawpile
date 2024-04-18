/*
 * SPDX-FileCopyrightText: 2013-2020 Mattia Basaglia
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef COLOR_DELEGATE_HPP
#define COLOR_DELEGATE_HPP

#include "colorwidgets_global.hpp"

#include <QStyledItemDelegate>

namespace color_widgets {


class QCP_EXPORT ReadOnlyColorDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
                    const QModelIndex &index) const Q_DECL_OVERRIDE;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const Q_DECL_OVERRIDE;

    void setSizeHintForColor(const QSize& size_hint);
    const QSize& sizeHintForColor() const;

protected:
    void paintItem(QPainter *painter, const QStyleOptionViewItem &option,
                    const QModelIndex &index, const QBrush& brush) const;
private:
    QSize size_hint{24, 16};
};

/**
    Delegate to use a ColorSelector in a color list
*/
class QCP_EXPORT ColorDelegate : public ReadOnlyColorDelegate
{
    Q_OBJECT
public:
    using ReadOnlyColorDelegate::ReadOnlyColorDelegate;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const Q_DECL_OVERRIDE;

    void setEditorData(QWidget *editor, const QModelIndex &index) const Q_DECL_OVERRIDE;

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                        const QModelIndex &index) const Q_DECL_OVERRIDE;

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const Q_DECL_OVERRIDE;

protected:
    bool eventFilter(QObject * watched, QEvent * event) Q_DECL_OVERRIDE;

private Q_SLOTS:
    void close_editor();
    void color_changed();
};

} // namespace color_widgets

#endif // COLOR_DELEGATE_HPP
