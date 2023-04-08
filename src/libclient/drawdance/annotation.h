// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_ANNOTATION_H
#define DRAWDANCE_ANNOTATION_H

#include <QByteArray>
#include <QColor>
#include <QRect>
#include <QString>

struct DP_Annotation;

namespace drawdance {

class Annotation final {
public:
    static Annotation null();
    static Annotation inc(DP_Annotation *a);
    static Annotation noinc(DP_Annotation *a);

    Annotation(const Annotation &other);
    Annotation(Annotation &&other);

    Annotation &operator=(const Annotation &other);
    Annotation &operator=(Annotation &&other);

    ~Annotation();

    bool isNull() const;

    int id() const;

    int x() const;
    int y() const;
    int width() const;
    int height() const;
    QRect bounds() const;

    bool protect() const;

    int valign() const;

    QColor backgroundColor() const;

    QByteArray textBytes() const;
    QString text() const;

private:
    explicit Annotation(DP_Annotation *a);

    DP_Annotation *m_data;
};

}

#endif
