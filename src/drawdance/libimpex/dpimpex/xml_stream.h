// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DPIMPEX_XML_STREAM
#define DPIMPEX_XML_STREAM
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

bool DP_xml_element_contains_namespace_declaration(DP_XmlElement *element,
                                                   const char *namespace_uri);


#endif
