/*
 * SPDX-FileCopyrightText: 2013-2020 Mattia Basaglia
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef ABSTRACT_WIDGET_LIST_HPP
#define ABSTRACT_WIDGET_LIST_HPP

#include "colorwidgets_global.hpp"

#include <QSignalMapper>
#include <QTableWidget>

class QCP_EXPORT AbstractWidgetList : public QWidget
{
    Q_OBJECT
public:
    explicit AbstractWidgetList(QWidget *parent = nullptr);
    ~AbstractWidgetList();
    
    /**
     *  \brief Get the number of items
     */
    int count() const;

    /**
     *  \brief Swap row a and row b
     */
    virtual void swap(int a, int b) = 0;


    /// Whether the given row index is valid
    bool isValidRow(int i) const { return i >= 0 && i < count(); }

    void setRowHeight(int row, int height);


public Q_SLOTS:
    /**
     *  \brief Remove row i
     */
    void remove(int i);

    /**
     *  \brief append a default row
     */
    virtual void append() = 0;

Q_SIGNALS:
    void removed(int i);

protected:

    /**
     *  \brief Create a new row with the given widget
     *
     *  Must be caled by implementations of append()
     */
    void appendWidget(QWidget* w);

    /**
     *  \brief get the widget found at the given row
     */
    QWidget* widget(int i);


    /**
     *  \brief get the widget found at the given row
     */
    template<class T>
    T* widget_cast(int i) { return qobject_cast<T*>(widget(i)); }

    /**
     *  \brief clear all rows without emitting signals
     *
     *  May be useful when implementation want to set all values at once
     */
    void clear();

private Q_SLOTS:
    void remove_clicked(QWidget* w);
    void up_clicked(QWidget* w);
    void down_clicked(QWidget* w);

private:
    class Private;
    Private * const p;

    QWidget* create_button(QWidget* data, QSignalMapper*mapper,
                           QString icon_name, QString text,
                           QString tooltip = QString()) const;
};

#endif // ABSTRACT_WIDGET_LIST_HPP
