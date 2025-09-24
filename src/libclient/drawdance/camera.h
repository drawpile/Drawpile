// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_DRAWDANCE_CAMERA_H
#define LIBCLIENT_DRAWDANCE_CAMERA_H
#include <QString>
#include <QSize>
#include <QRect>

struct DP_Camera;

namespace drawdance {

class Camera final {
public:
	static Camera null();
	static Camera inc(DP_Camera *c);
	static Camera noinc(DP_Camera *c);

	Camera() {}
	Camera(const Camera &other);
	Camera(Camera &&other);

	Camera &operator=(const Camera &other);
	Camera &operator=(Camera &&other);

	~Camera();

	bool isNull() const;

	bool hidden() const;

	int id() const;

	unsigned int flags() const;

	int interpolation() const;

	double effectiveFramerate() const;
	bool frameratesValid() const;

	int rangeFirst() const;
	int rangeLast() const;
	bool rangeValid() const;

	QSize outputSize() const;
	bool outputValid() const;

	QRect viewport() const;
	bool viewportValid() const;

	QString title() const;

private:
	explicit Camera(DP_Camera *c);

	DP_Camera *m_data;
};

}

#endif
