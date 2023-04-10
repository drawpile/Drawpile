// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_TRACK_H
#define DRAWDANCE_TRACK_H

#include "libclient/drawdance/keyframe.h"
#include <QString>

struct DP_Track;

namespace drawdance {

class Track final {
public:
	static Track null();
	static Track inc(DP_Track *t);
	static Track noinc(DP_Track *t);

	Track() {}
	Track(const Track &other);
	Track(Track &&other);

	Track &operator=(const Track &other);
	Track &operator=(Track &&other);

	~Track();

	bool isNull() const;

	int id() const;

	QString title() const;

	bool hidden() const;

	bool onionSkin() const;

	int keyFrameCount() const;

	KeyFrame keyFrameAt(int index) const;

	int frameIndexAt(int index) const;

private:
	explicit Track(DP_Track *tl);

	DP_Track *m_data;
};

}

#endif
