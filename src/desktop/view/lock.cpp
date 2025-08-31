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

	if(hasReason(Reason::NoFillSource)) {
		lines.append(tr("You need to set a layer as the fill source"));
	}

	if(hasReason(Reason::LayerGroup)) {
		lines.append(tr("Layer is a group"));
	} else if(hasReason(Reason::LayerLocked)) {
		lines.append(tr("Layer is locked"));
	} else if(hasReason(Reason::LayerCensoredRemote)) {
		if(hasReason(Reason::LayerCensoredLocal)) {
			lines.append(tr("Layer is censored and blocked"));
		} else {
			lines.append(tr("Layer is censored"));
		}
	} else if(hasReason(Reason::LayerCensoredLocal)) {
		lines.append(tr("Layer is blocked"));
	} else if(hasReason(Reason::LayerHidden)) {
		lines.append(tr("Layer is hidden"));
	} else if(hasReason(Reason::LayerHiddenInFrame)) {
		lines.append(tr("Layer is not visible in this frame"));
	} else if(hasReason(Reason::NoLayer)) {
		lines.append(tr("No layer selected"));
	} else if(hasReason(Reason::OverlappingFillSource)) {
		lines.append(tr("Choose a different layer to fill on"));
	}

	if(hasReason(Reason::Tool)) {
		lines.append(tr("Tool is locked"));
	}

	if(hasReason(Reason::NoSelection)) {
		lines.append(tr("Tool requires a selection"));
	}

	return lines.join(QStringLiteral("\n"));
}

}
