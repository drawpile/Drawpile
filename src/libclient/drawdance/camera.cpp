// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpcommon/geom.h>
#include <dpengine/camera.h>
}
#include "libclient/drawdance/camera.h"
#include "libshared/util/qtcompat.h"

namespace drawdance {

Camera Camera::null()
{
	return Camera();
}

Camera Camera::inc(DP_Camera *tl)
{
	return Camera(DP_camera_incref_nullable(tl));
}

Camera Camera::noinc(DP_Camera *tl)
{
	return Camera{tl};
}

Camera::Camera(const Camera &other)
	: Camera(DP_camera_incref_nullable(other.m_data))
{
}

Camera::Camera(Camera &&other)
	: Camera(other.m_data)
{
	other.m_data = nullptr;
}

Camera &Camera::operator=(const Camera &other)
{
	DP_camera_decref_nullable(m_data);
	m_data = DP_camera_incref_nullable(other.m_data);
	return *this;
}

Camera &Camera::operator=(Camera &&other)
{
	DP_camera_decref_nullable(m_data);
	m_data = other.m_data;
	other.m_data = nullptr;
	return *this;
}

Camera::~Camera()
{
	DP_camera_decref_nullable(m_data);
}

bool Camera::isNull() const
{
	return !m_data;
}

bool Camera::hidden() const
{
	return DP_camera_hidden(m_data);
}

int Camera::id() const
{
	return DP_camera_id(m_data);
}

unsigned int Camera::flags() const
{
	return DP_camera_flags(m_data);
}

int Camera::interpolation() const
{
	return DP_camera_interpolation(m_data);
}

double Camera::effectiveFramerate() const
{
	return DP_camera_effective_framerate(m_data);
}

bool Camera::frameratesValid() const
{
	return DP_camera_framerates_valid(m_data);
}

int Camera::rangeFirst() const
{
	return DP_camera_range_first(m_data);
}

int Camera::rangeLast() const
{
	return DP_camera_range_last(m_data);
}

bool Camera::rangeValid() const
{
	return DP_camera_range_valid(m_data);
}

QSize Camera::outputSize() const
{
	return QSize(
		DP_camera_output_width(m_data), DP_camera_output_height(m_data));
}

bool Camera::outputValid() const
{
	return DP_camera_output_valid(m_data);
}

QRect Camera::viewport() const
{
	DP_Rect viewport = *DP_camera_viewport(m_data);
	if(DP_rect_valid(viewport)) {
		return QRect(
			QPoint(viewport.x1, viewport.y1), QPoint(viewport.x2, viewport.y2));
	} else {
		return QRect();
	}
}

bool Camera::viewportValid() const
{
	return DP_camera_viewport_valid(m_data);
}

QString Camera::title() const
{
	size_t length;
	const char *title = DP_camera_title(m_data, &length);
	return QString::fromUtf8(title, compat::castSize(length));
}

Camera::Camera(DP_Camera *tl)
	: m_data(tl)
{
}

}
