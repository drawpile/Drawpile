// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/view/lock.h"

namespace view {

Lock::Lock(QObject *parent)
	: QObject(parent)
{
}

void Lock::setReasons(QFlags<Reason> reasons)
{
	if(reasons != m_reasons) {
		m_reasons = reasons;
		emit reasonsChanged(reasons);
		QString description = buildDescription();
		if(description != m_description) {
			m_description = description;
			emit descriptionChanged(m_description);
		}
	}
}

QString Lock::buildDescription() const
{
	QStringList lines;

	if(hasReason(Reason::OutOfSpace)) {
		lines.append(tr("Out of space, use Session > Reset to fix this"));
	}

	if(hasReason(Reason::Reset)) {
		lines.append(tr("Reset in progress"));
	}

	if(hasReason(Reason::Canvas)) {
		lines.append(tr("Canvas is locked"));
	}

	if(hasReason(Reason::User)) {
		lines.append(tr("User is locked"));
	}

	if(hasReason(Reason::LayerGroup)) {
		lines.append(tr("Layer is a group"));
	} else if(hasReason(Reason::LayerLocked)) {
		lines.append(tr("Layer is locked"));
	} else if(hasReason(Reason::LayerCensored)) {
		lines.append(tr("Layer is censored"));
	} else if(hasReason(Reason::LayerHidden)) {
		lines.append(tr("Layer is hidden"));
	} else if(hasReason(Reason::LayerHiddenInFrame)) {
		lines.append(tr("Layer is not visible in this frame"));
	} else if(hasReason(Reason::NoLayer)) {
		lines.append(tr("No layer selected"));
	}

	if(hasReason(Reason::Tool)) {
		lines.append(tr("Tool is locked"));
	}

	return lines.join(QStringLiteral("\n"));
}

}
