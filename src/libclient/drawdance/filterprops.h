// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_DRAWDANCE_FILTER_PROPS_H
#define LIBCLIENT_DRAWDANCE_FILTER_PROPS_H
#include <QByteArray>

struct DP_FilterProps;

namespace drawdance {

class FilterProps final {
public:
	static FilterProps makeHsvAdjust(float h, float s, float v);
	static FilterProps makeHslAdjust(float h, float s, float l);
	static FilterProps makeHcyAdjust(float h, float c, float y);

	static FilterProps null();
	static FilterProps inc(DP_FilterProps *lp);
	static FilterProps noinc(DP_FilterProps *lp);

	FilterProps(const FilterProps &other);
	FilterProps(FilterProps &&other);

	FilterProps &operator=(const FilterProps &other);
	FilterProps &operator=(FilterProps &&other);

	~FilterProps();

	DP_FilterProps *get() const;

	bool isNull() const;

	QByteArray serialize();

private:
	explicit FilterProps(DP_FilterProps *fp);

	static unsigned char *getSerializeBuffer(void *user, size_t size);

	DP_FilterProps *m_data;
};

}

#endif
