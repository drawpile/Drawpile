// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/brushes/lazybrush.h"
#include <QJsonDocument>

namespace brushes {

LazyBrush LazyBrush::fromBytes(const QByteArray &bytes)
{
	LazyBrush lb;
	lb.setBytes(bytes);
	return lb;
}

LazyBrush LazyBrush::fromBytes(QByteArray &&bytes)
{
	LazyBrush lb;
	lb.setBytes(std::move(bytes));
	return lb;
}

LazyBrush LazyBrush::fromBrush(const ActiveBrush &brush)
{
	LazyBrush lb;
	lb.setBrush(brush);
	return lb;
}

LazyBrush LazyBrush::fromBrush(ActiveBrush &&brush)
{
	LazyBrush lb;
	lb.setBrush(std::move(brush));
	return lb;
}

void LazyBrush::clear()
{
	m_state = STATE_BLANK;
	m_bytes.clear();
}

void LazyBrush::setBytes(const QByteArray &bytes)
{
	m_state = STATE_BYTES;
	m_bytes = bytes;
}

void LazyBrush::setBytes(QByteArray &&bytes)
{
	m_state = STATE_BYTES;
	m_bytes = std::move(bytes);
}

void LazyBrush::setBrush(const ActiveBrush &brush)
{
	m_state = STATE_BRUSH;
	m_bytes.clear();
	m_brush = brush;
}

void LazyBrush::setBrush(ActiveBrush &&brush)
{
	m_state = STATE_BRUSH;
	m_bytes.clear();
	m_brush = std::move(brush);
}

void LazyBrush::loadBrush(int presetId) const
{
	if(m_state == STATE_BYTES) {
		m_state = STATE_BRUSH;
		QJsonParseError err;
		QJsonDocument doc = QJsonDocument::fromJson(m_bytes, &err);
		m_bytes.clear();
		if(err.error != QJsonParseError::NoError) {
			qWarning(
				"Error parsing preset %d: %s", presetId,
				qUtf8Printable(err.errorString()));
			m_brush = ActiveBrush();
		} else if(!doc.isObject()) {
			qWarning("Data for preset %d is not a JSON object", presetId);
			m_brush = ActiveBrush();
		} else {
			m_brush = ActiveBrush::fromJson(doc.object());
		}
	}
}

}
