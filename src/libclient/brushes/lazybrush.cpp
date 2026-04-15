// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/brushes/lazybrush.h"
#include <QJsonDocument>

namespace brushes {

LazyBrush LazyBrush::fromLoader(LoaderFn fn, void *user)
{
	LazyBrush lb;
	lb.setLoader(fn, user);
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
	m_loaderFn = nullptr;
}

void LazyBrush::setLoader(LoaderFn fn, void *user)
{
	m_loaderFn = fn;
	m_loaderUser = user;
}

void LazyBrush::setBrush(const ActiveBrush &brush)
{
	m_loaderFn = nullptr;
	m_brush = brush;
}

void LazyBrush::setBrush(ActiveBrush &&brush)
{
	m_loaderFn = nullptr;
	m_brush = std::move(brush);
}

void LazyBrush::loadBrush(int presetId) const
{
	if(LoaderFn loaderFn = m_loaderFn; loaderFn) {
		m_loaderFn = nullptr;
		QJsonParseError err;
		QJsonDocument doc =
			QJsonDocument::fromJson(loaderFn(m_loaderUser, presetId), &err);
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
