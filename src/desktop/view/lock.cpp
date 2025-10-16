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
	, m_selectAllAction(new QAction(
		  QIcon::fromTheme(QStringLiteral("edit-select-all")), tr("Select all"),
		  this))
	, m_selectLayerBoundsAction(new QAction(
		  QIcon::fromTheme(QStringLiteral("select-rectangular")),
		  tr("Select layer bounds"), this))
	, m_setFillSourceAction(new QAction(
		  QIcon::fromTheme(QStringLiteral("tag")),
		  tr("Set current layer as fill source"), this))
	, m_clearFillSourceAction(new QAction(
		  QIcon::fromTheme(QStringLiteral("tag-delete")),
		  tr("Clear fill source"), this))
	, m_uncensorLayersAction(new QAction(
		  QIcon::fromTheme(QStringLiteral("view-visible")),
		  tr("Show censored layers"), this))
	, m_viewMode(int(DP_VIEW_MODE_NORMAL))
{
}

bool Lock::updateReasons(
	QFlags<Reason> activeReasons, QFlags<Reason> allReasons, int viewMode,
	bool op, bool canUncensor)
{
	if(activeReasons != m_activeReasons || allReasons != m_allReasons ||
	   m_viewMode != viewMode || m_op != op || m_canUncensor != canUncensor) {
		m_activeReasons = activeReasons;
		m_allReasons = allReasons;
		m_viewMode = viewMode;
		m_op = op;
		m_canUncensor = canUncensor;
		buildDescriptions();
		buildActions();
		emit lockStateChanged(m_activeReasons, m_descriptions, m_actions);
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
		emit lockStateChanged(m_activeReasons, m_descriptions, m_actions);
	}
}

void Lock::buildDescriptions()
{
	m_descriptions.clear();

	if(hasAny(Reason::OutOfSpace)) {
		if(m_op) {
			m_descriptions.append(
				tr("Out of space, you must reset the canvas"));
		} else {
			m_descriptions.append(
				tr("Out of space, operator must reset the canvas"));
		}
	}

	if(hasAny(Reason::Reset)) {
		m_descriptions.append(tr("Reset in progress"));
	}

	if(hasAny(Reason::Canvas)) {
		m_descriptions.append(tr("Canvas is locked"));
	}

	if(hasAny(Reason::User)) {
		m_descriptions.append(tr("User is locked"));
	}

	if(hasAny(Reason::NoSelection)) {
		m_descriptions.append(tr("Tool requires a selection"));
	}

	if(hasAny(Reason::NoFillSource)) {
		m_descriptions.append(tr("You need to set a layer as the fill source"));
	}

	if(hasAny(Reason::LayerGroup)) {
		m_descriptions.append(tr("Layer is a group"));
	} else if(hasAny(Reason::LayerLocked)) {
		m_descriptions.append(tr("Layer is locked"));
	} else if(hasAny(Reason::LayerCensoredRemote)) {
		if(hasAny(Reason::LayerCensoredLocal)) {
			m_descriptions.append(tr("Layer is censored and blocked"));
		} else {
			m_descriptions.append(tr("Layer is censored"));
		}
	} else if(hasAny(Reason::LayerCensoredLocal)) {
		m_descriptions.append(tr("Layer is blocked"));
	} else if(hasAny(Reason::LayerHidden)) {
		m_descriptions.append(tr("Layer is hidden"));
	} else if(hasAny(Reason::NoLayer)) {
		m_descriptions.append(tr("No layer selected"));
	} else if(hasAny(Reason::OverlappingFillSource)) {
		m_descriptions.append(tr("Choose a different layer to fill on"));
	}

	if(hasAny(Reason::LayerHiddenInFrame)) {
		m_descriptions.append(tr("Layer is not visible in this frame"));
	}

	if(hasAny(Reason::Tool)) {
		m_descriptions.append(tr("Tool is locked"));
	}
}

void Lock::buildActions()
{
	m_actions.clear();

	if(!hasAny(Reason::Reset)) {
		if(hasAny(Reason::OutOfSpace)) {
			if(m_op) {
				m_actions.append(m_resetCanvasAction);
			}
		} else if(hasAny(Reason::Canvas)) {
			if(m_op) {
				m_actions.append(m_unlockCanvasAction);
			}
		} else if(hasAny(Reason::NoSelection) && !hasAny(Reason::User)) {
			m_actions.append(m_selectAllAction);
			m_actions.append(m_selectLayerBoundsAction);
		}

		if(hasAny(Reason::NoFillSource) && !hasAny(Reason::NoLayer)) {
			m_actions.append(m_setFillSourceAction);
		} else if(hasAny(Reason::OverlappingFillSource)) {
			m_actions.append(m_clearFillSourceAction);
		}
	}

	if(hasAny(Reason::LayerCensoredRemote) && m_canUncensor) {
		m_actions.append(m_uncensorLayersAction);
	}

	if(m_showViewModeNotices) {
		switch(m_viewMode) {
		case int(DP_VIEW_MODE_LAYER):
			m_actions.append(m_exitLayerViewModeAction);
			break;
		case int(DP_VIEW_MODE_GROUP):
			m_actions.append(m_exitGroupViewModeAction);
			break;
		case int(DP_VIEW_MODE_FRAME):
			if(hasAny(Reason::LayerHiddenInFrame)) {
				m_actions.append(m_exitFrameViewModeAction);
			}
			break;
		default:
			break;
		}
	}
}

}
