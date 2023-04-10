// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpengine/key_frame.h>
}

#include "libclient/drawdance/keyframe.h"
#include "libshared/util/qtcompat.h"

namespace drawdance {

KeyFrame KeyFrame::null()
{
	return KeyFrame{};
}

KeyFrame KeyFrame::inc(DP_KeyFrame *tl)
{
	return KeyFrame{DP_key_frame_incref_nullable(tl)};
}

KeyFrame KeyFrame::noinc(DP_KeyFrame *tl)
{
	return KeyFrame{tl};
}

KeyFrame::KeyFrame(const KeyFrame &other)
	: KeyFrame{DP_key_frame_incref_nullable(other.m_data)}
{
}

KeyFrame::KeyFrame(KeyFrame &&other)
	: KeyFrame{other.m_data}
{
	other.m_data = nullptr;
}

KeyFrame &KeyFrame::operator=(const KeyFrame &other)
{
	DP_key_frame_decref_nullable(m_data);
	m_data = DP_key_frame_incref_nullable(other.m_data);
	return *this;
}

KeyFrame &KeyFrame::operator=(KeyFrame &&other)
{
	DP_key_frame_decref_nullable(m_data);
	m_data = other.m_data;
	other.m_data = nullptr;
	return *this;
}

KeyFrame::~KeyFrame()
{
	DP_key_frame_decref_nullable(m_data);
}

bool KeyFrame::isNull() const
{
	return !m_data;
}

int KeyFrame::layerId() const
{
	return DP_key_frame_layer_id(m_data);
}

QString KeyFrame::title() const
{
	size_t length;
	const char *title = DP_key_frame_title(m_data, &length);
	return QString::fromUtf8(title, compat::castSize(length));
}

QVector<DP_KeyFrameLayer> KeyFrame::layers() const
{
	int count;
	const DP_KeyFrameLayer *layers = DP_key_frame_layers(m_data, &count);
	return QVector<DP_KeyFrameLayer>(layers, layers + count);
}

KeyFrame::KeyFrame(DP_KeyFrame *tl)
	: m_data{tl}
{
}

}
