// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_KEYFRAME_H
#define DRAWDANCE_KEYFRAME_H

#include <QString>
#include <QVector>

struct DP_KeyFrame;
struct DP_KeyFrameLayer;

namespace drawdance {

class KeyFrame final {
public:
	static KeyFrame null();
	static KeyFrame inc(DP_KeyFrame *kf);
	static KeyFrame noinc(DP_KeyFrame *kf);

	KeyFrame() {}
	KeyFrame(const KeyFrame &other);
	KeyFrame(KeyFrame &&other);

	KeyFrame &operator=(const KeyFrame &other);
	KeyFrame &operator=(KeyFrame &&other);

	~KeyFrame();

	bool isNull() const;

	int layerId() const;

	QString title() const;

	QVector<DP_KeyFrameLayer> layers() const;

private:
	explicit KeyFrame(DP_KeyFrame *kf);

	DP_KeyFrame *m_data;
};

}

#endif
