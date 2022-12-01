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
#ifndef DPENGINE_XML_STREAM
#define DPENGINE_XML_STREAM
#include <dpcommon/common.h>


typedef struct DP_XmlElement DP_XmlElement;

typedef bool (*DP_XmlStartElementFn)(void *user, DP_XmlElement *element);
typedef bool (*DP_XmlTextContentFn)(void *user, size_t len, const char *text);
typedef bool (*DP_XmlEndElementFn)(void *user);


bool DP_xml_stream(size_t size, const char *buffer,
                   DP_XmlStartElementFn on_start_element,
                   DP_XmlTextContentFn on_text_content,
                   DP_XmlEndElementFn on_end_element, void *user);


const char *DP_xml_element_name(DP_XmlElement *element);

const char *DP_xml_element_namespace(DP_XmlElement *element);

bool DP_xml_element_name_equals(DP_XmlElement *element,
                                const char *namespace_or_null,
                                const char *name);

const char *DP_xml_element_attribute(DP_XmlElement *element,
                                     const char *namespace_or_null,
                                     const char *name);


#endif
