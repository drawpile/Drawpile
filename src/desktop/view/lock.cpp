// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/view_mode.h>
}
#include "desktop/view/lock.h"
#include <QAction>
#include <QCoreApplication>
#include <QIcon>

namespace view {

Lock::Lock(QObject *parent)
	: QObject(parent)
	, m_exitLayerViewModeAction(new QAction(
		  QIcon::fromTheme(QStringLiteral("drawpile_close")),
		  tr("Exit layer view"), this))
	, m_exitGroupViewModeAction(new QAction(
		  QIcon::fromTheme(QStringLiteral("drawpile_close")),
		  tr("Exit group view"), this))
	, m_exitFrameViewModeAction(new QAction(
		  QIcon::fromTheme(QStringLiteral("drawpile_close")),
		  tr("Exit frame view"), this))
	, m_unlockCanvasAction(new QAction(
		  QIcon::fromTheme(QStringLiteral("object-unlocked")),
		  tr("Unlock canvas"), this))
	, m_resetCanvasAction(new QAction(
		  QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Reset canvas…"),
		  this))
	, m_uncensorLayersAction(new QAction(
		  QIcon::fromTheme(QStringLiteral("view-visible")),
		  tr("Show censored layers"), this))
	, m_viewMode(int(DP_VIEW_MODE_NORMAL))
{
}

bool Lock::updateReasons(
	QFlags<Reason> reasons, int viewMode, bool op, bool canUncensor)
{
	if(reasons != m_reasons || m_viewMode != viewMode || m_op != op ||
	   m_canUncensor != canUncensor) {
		m_reasons = reasons;
		m_viewMode = viewMode;
		m_op = op;
		m_canUncensor = canUncensor;
		buildDescriptions();
		buildActions();
		emit lockStateChanged(m_reasons, m_descriptions, m_actions);
		return true;
	} else {
		return false;
	}
}

QString Lock::description() const
{
	return m_descriptions.join('\n');
}

void Lock::setShowVewModeNotices(bool showViewModeNotices)
{
	if(showViewModeNotices != m_showViewModeNotices) {
		m_showViewModeNotices = showViewModeNotices;
		buildDescriptions();
		buildActions();
		emit lockStateChanged(m_reasons, m_descriptions, m_actions);
	}
}

void Lock::buildDescriptions()
{
	m_descriptions.clear();

	if(m_showViewModeNotices) {
		switch(m_viewMode) {
		case int(DP_VIEW_MODE_LAYER):
			m_descriptions.append(tr("View Mode: Layer"));
			break;
		case int(DP_VIEW_MODE_GROUP):
			m_descriptions.append(tr("View Mode: Group"));
			break;
		default:
			break;
		}
	}

	if(hasReason(Reason::OutOfSpace)) {
		if(m_op) {
			m_descriptions.append(
				tr("Out of space, you must reset the canvas"));
		} else {
			m_descriptions.append(
				tr("Out of space, operator must reset the canvas"));
		}
	}

	if(hasReason(Reason::Reset)) {
		m_descriptions.append(tr("Reset in progress"));
	}

	if(hasReason(Reason::Canvas)) {
		m_descriptions.append(tr("Canvas is locked"));
	}

	if(hasReason(Reason::User)) {
		m_descriptions.append(tr("User is locked"));
	}

	if(hasReason(Reason::NoFillSource)) {
		m_descriptions.append(tr("You need to set a layer as the fill source"));
	}

	if(hasReason(Reason::LayerGroup)) {
		m_descriptions.append(tr("Layer is a group"));
	} else if(hasReason(Reason::LayerLocked)) {
		m_descriptions.append(tr("Layer is locked"));
	} else if(hasReason(Reason::LayerCensoredRemote)) {
		if(hasReason(Reason::LayerCensoredLocal)) {
			m_descriptions.append(tr("Layer is censored and blocked"));
		} else {
			m_descriptions.append(tr("Layer is censored"));
		}
	} else if(hasReason(Reason::LayerCensoredLocal)) {
		m_descriptions.append(tr("Layer is blocked"));
	} else if(hasReason(Reason::LayerHidden)) {
		m_descriptions.append(tr("Layer is hidden"));
	} else if(hasReason(Reason::NoLayer)) {
		m_descriptions.append(tr("No layer selected"));
	} else if(hasReason(Reason::OverlappingFillSource)) {
		m_descriptions.append(tr("Choose a different layer to fill on"));
	}

	if(hasReason(Reason::LayerHiddenInFrame)) {
		m_descriptions.append(tr("Layer is not visible in this frame"));
	}

	if(hasReason(Reason::Tool)) {
		m_descriptions.append(tr("Tool is locked"));
	}

	if(hasReason(Reason::NoSelection)) {
		m_descriptions.append(tr("Tool requires a selection"));
	}
}

void Lock::buildActions()
{
	m_actions.clear();

	if(m_showViewModeNotices) {
		switch(m_viewMode) {
		case int(DP_VIEW_MODE_LAYER):
			m_actions.append(m_exitLayerViewModeAction);
			break;
		case int(DP_VIEW_MODE_GROUP):
			m_actions.append(m_exitGroupViewModeAction);
			break;
		case int(DP_VIEW_MODE_FRAME):
			if(hasReason(Reason::LayerHiddenInFrame)) {
				m_actions.append(m_exitFrameViewModeAction);
			}
			break;
		default:
			break;
		}
	}

	if(hasReason(Reason::OutOfSpace)) {
		if(m_op) {
			m_actions.append(m_resetCanvasAction);
		}
	} else if(hasReason(Reason::Canvas)) {
		if(m_op) {
			m_actions.append(m_unlockCanvasAction);
		}
	}

	if(hasReason(Reason::LayerCensoredRemote) && m_canUncensor) {
		m_actions.append(m_uncensorLayersAction);
	}
}

}
