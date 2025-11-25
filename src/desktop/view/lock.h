// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_VIEW_LOCK_H
#define DESKTOP_VIEW_LOCK_H
#include <QObject>
#include <QVector>

class QAction;

namespace view {

class Lock : public QObject {
	Q_OBJECT
public:
	enum class Reason : unsigned int {
		None = 0u,
		Reset = 1u << 0u,
		Canvas = 1u << 1u,
		User = 1u << 2u,
		LayerLocked = 1u << 3u,
		LayerGroup = 1u << 4u,
		LayerCensoredRemote = 1u << 5u,
		LayerCensoredLocal = 1u << 6u,
		LayerHidden = 1u << 7u,
		LayerHiddenInFrame = 1u << 8u,
		NoLayer = 1u << 9u,
		Tool = 1u << 10u,
		OutOfSpace = 1u << 11u,
		NoSelection = 1u << 12u,
		NoFillSource = 1u << 13u,
		OverlappingFillSource = 1u << 14u,
	};
	Q_ENUM(Reason)

	explicit Lock(QObject *parent = nullptr);

	QAction *exitLayerViewModeAction() { return m_exitLayerViewModeAction; }
	QAction *exitGroupViewModeAction() { return m_exitGroupViewModeAction; }
	QAction *exitFrameViewModeAction() { return m_exitFrameViewModeAction; }
	QAction *unlockCanvasAction() { return m_unlockCanvasAction; }
	QAction *resetCanvasAction() { return m_resetCanvasAction; }
	QAction *selectAllAction() { return m_selectAllAction; }
	QAction *selectLayerBoundsAction() { return m_selectLayerBoundsAction; }
	QAction *disableAntiOverflowAction() { return m_disableAntiOverflowAction; }
	QAction *setFillSourceAction() { return m_setFillSourceAction; }
	QAction *clearFillSourceAction() { return m_clearFillSourceAction; }
	QAction *uncensorLayersAction() { return m_uncensorLayersAction; }

	bool updateReasons(
		QFlags<Reason> activeReasons, QFlags<Reason> allReasons, int viewMode,
		bool op, bool canUncensor);

	bool isLocked() const { return m_activeReasons; }
	QString description() const;

	void setShowViewModeNotices(bool showViewModeNotices);

signals:
	void lockStateChanged(
		QFlags<Reason> reasons, const QStringList &descriptions,
		const QVector<QAction *> &actions);

private:
	void buildDescriptions();
	void buildActions();

	bool hasAny(Reason reason) const { return m_allReasons.testFlag(reason); }

	QAction *const m_exitLayerViewModeAction;
	QAction *const m_exitGroupViewModeAction;
	QAction *const m_exitFrameViewModeAction;
	QAction *const m_unlockCanvasAction;
	QAction *const m_resetCanvasAction;
	QAction *const m_selectAllAction;
	QAction *const m_selectLayerBoundsAction;
	QAction *const m_disableAntiOverflowAction;
	QAction *const m_setFillSourceAction;
	QAction *const m_clearFillSourceAction;
	QAction *const m_uncensorLayersAction;
	QFlags<Reason> m_activeReasons;
	QFlags<Reason> m_allReasons;
	QStringList m_descriptions;
	QVector<QAction *> m_actions;
	int m_viewMode;
	bool m_op = false;
	bool m_canUncensor = true;
	bool m_showViewModeNotices = true;
};

}

#endif
