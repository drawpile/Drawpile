/*
 * Copyright (C) 2022 askmeaboufoom
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
extern "C" {
#include "xml_stream.h"
#include <dpcommon/common.h>
#include <dpcommon/conversions.h>
}
#include <QByteArray>
#include <QHash>
#include <QXmlStreamReader>


struct DP_XmlElement {
  public:
    DP_XmlElement(QXmlStreamReader *xsr)
        : m_xsr{xsr}, m_name{}, m_namespace_uri{}, m_attributes{}
    {
    }

    DP_XmlElement(const DP_XmlElement &) = delete;
    DP_XmlElement(DP_XmlElement &&) = delete;
    DP_XmlElement &operator=(const DP_XmlElement &) = delete;
    DP_XmlElement &operator=(DP_XmlElement &&) = delete;

    const char *name()
    {
        if (m_name.isNull()) {
            m_name = m_xsr->name().toUtf8();
        }
        return m_name.constData();
    }

    const char *namespace_uri()
    {
        if (m_namespace_uri.isNull()) {
            m_namespace_uri = m_xsr->namespaceUri().toUtf8();
        }
        return m_namespace_uri.constData();
    }

    bool name_equals(const QString &namespace_uri, const QString &name)
    {
        return m_xsr->namespaceUri() == namespace_uri && m_xsr->name() == name;
    }

    const char *attribute(const QString &namespace_uri, const QString &name)
    {
        QXmlStreamAttributes attributes = m_xsr->attributes();
        if (attributes.hasAttribute(namespace_uri, name)) {
            QPair<QString, QString> key{namespace_uri, name};
            if (!m_attributes.contains(key)) {
                m_attributes.insert(
                    key, attributes.value(namespace_uri, name).toUtf8());
            }
            return m_attributes.value(key).constData();
        }
        else {
            return nullptr;
        }
    }

  private:
    QXmlStreamReader *m_xsr;
    QByteArray m_name;
    QByteArray m_namespace_uri;
    QHash<QPair<QString, QString>, QByteArray> m_attributes;
};

static bool call_on_start_element(QXmlStreamReader *xsr,
                                  DP_XmlStartElementFn on_start_element,
                                  void *user)
{
    if (on_start_element) {
        DP_XmlElement element{xsr};
        return on_start_element(user, &element);
    }
    else {
        return true;
    }
}

static bool call_on_text_content(QXmlStreamReader *xsr,
                                 DP_XmlTextContentFn on_text_content,
                                 void *user)
{
    if (on_text_content) {
        QByteArray text = xsr->text().toUtf8();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        size_t len = DP_int_to_size(text.size());
#else
        size_t len = (size_t)text.size();
#endif
        return on_text_content(user, len, text.constData());
    }
    else {
        return true;
    }
}

static bool call_on_end_element(DP_XmlEndElementFn on_end_element, void *user)
{
    if (on_end_element) {
        return on_end_element(user);
    }
    else {
        return true;
    }
}

extern "C" bool DP_xml_stream(size_t size, const char *buffer,
                              DP_XmlStartElementFn on_start_element,
                              DP_XmlTextContentFn on_text_content,
                              DP_XmlEndElementFn on_end_element, void *user)
{
    QXmlStreamReader xsr{QByteArray::fromRawData(buffer, DP_size_to_int(size))};
    while (!xsr.atEnd()) {
        switch (xsr.readNext()) {
        case QXmlStreamReader::StartElement:
            if (!call_on_start_element(&xsr, on_start_element, user)) {
                return false;
            }
            break;
        case QXmlStreamReader::Characters:
            if (!call_on_text_content(&xsr, on_text_content, user)) {
                return false;
            }
            break;
        case QXmlStreamReader::EndElement:
            if (!call_on_end_element(on_end_element, user)) {
                return false;
            }
            break;
        default:
            break;
        }
    }

    if (xsr.hasError()) {
        QByteArray error = xsr.errorString().toUtf8();
        DP_error_set("Error reading XML: %s", error.constData());
        return false;
    }
    else {
        return true;
    }
}


extern "C" const char *DP_xml_element_name(DP_XmlElement *element)
{
    DP_ASSERT(element);
    const char *name = element->name();
    return name ? name : "";
}

extern "C" const char *DP_xml_element_namespace(DP_XmlElement *element)
{
    DP_ASSERT(element);
    const char *namespace_uri = element->namespace_uri();
    return namespace_uri ? namespace_uri : "";
}

extern "C" bool DP_xml_element_name_equals(DP_XmlElement *element,
                                           const char *namespace_or_null,
                                           const char *name)
{
    DP_ASSERT(element);
    DP_ASSERT(name);
    return element->name_equals(
        namespace_or_null ? QString::fromUtf8(namespace_or_null) : QString{},
        QString::fromUtf8(name));
}

extern "C" const char *DP_xml_element_attribute(DP_XmlElement *element,
                                                const char *namespace_or_null,
                                                const char *name)
{
    DP_ASSERT(element);
    DP_ASSERT(name);
    return element->attribute(
        namespace_or_null ? QString::fromUtf8(namespace_or_null) : QString{},
        QString::fromUtf8(name));
}
