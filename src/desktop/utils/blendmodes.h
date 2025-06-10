// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_UTILS_BLENDMODES_H
#define DESKTOP_UTILS_BLENDMODES_H
#include "libclient/canvas/blendmodes.h"
#include <QObject>
#include <functional>

class QAbstractButton;
class QComboBox;
class QStandardItemModel;


int searchBlendModeComboIndex(QComboBox *combo, int mode);

QStandardItemModel *getLayerBlendModesFor(
	bool group, bool clip, bool automaticAlphaPreserve, bool compatibilityMode);

QStandardItemModel *getBrushBlendModesFor(
	bool erase, bool automaticAlphaPreserve, bool compatibilityMode,
	bool myPaint);

QStandardItemModel *
getFillBlendModesFor(bool automaticAlphaPreserve, bool compatibilityMode);


class BlendModeManager final : public QObject {
	Q_OBJECT
public:
	static BlendModeManager *initBrush(
		QComboBox *blendModeCombo, QComboBox *eraseModeCombo,
		QAbstractButton *alphaPreserveButton, QAbstractButton *eraseModeButton,
		QObject *parent);
	static BlendModeManager *initFill(
		QComboBox *blendModeCombo, QAbstractButton *alphaPreserveButton,
		QObject *parent);

	BlendModeManager(const BlendModeManager &) = delete;
	BlendModeManager(BlendModeManager &&) = delete;
	BlendModeManager &operator=(const BlendModeManager &) = delete;
	BlendModeManager &operator=(BlendModeManager &&) = delete;

	void setAutomaticAlphaPerserve(bool automaticAlphaPreserve);
	void setCompatibilityMode(bool compatibilityMode);
	void setMyPaint(bool myPaint);

	int getCurrentBlendMode() const;
	bool selectBlendMode(int blendMode);

	bool isEraseMode() const { return m_eraseMode; }
	void setEraseMode(bool eraseMode);

	void toggleAlphaPreserve();
	void toggleEraserMode();
	void toggleBlendMode(int blendMode);

signals:
	void blendModeChanged(int blendMode, bool eraseMode);

private:
	enum class Type { Brush, Fill };

	class SignalBlocker final {
	public:
		SignalBlocker(BlendModeManager *bmm);
		~SignalBlocker();

	private:
		static QObject *block(QObject *object);
		static void unblock(QObject *object);

		QObject *const m_blendModeManager;
		QObject *const m_blendModeCombo;
		QObject *const m_eraseModeCombo;
		QObject *const m_alphaPreserveButton;
		QObject *const m_eraseModeButton;
	};
	friend class ::BlendModeManager::SignalBlocker;

	BlendModeManager(
		Type type, QComboBox *blendModeCombo, QComboBox *eraseModeCombo,
		QAbstractButton *alphaPreserveButton, QAbstractButton *eraseModeButton,
		QObject *parent);

	void initBlendModeOptions();
	void reinitBlendModeOptions();

	void updateBlendModeIndex(int index);
	void updateAlphaPreserve(bool alphaPreserve);
	void updateEraseModeIndex(int index);

	void addToHistory(int blendMode);
	int searchHistory(
		const std::function<bool(int)> &predicate, int fallback) const;

	void selectBlendModeFromAlphaPreserve(int blendMode);

	bool isModeSelected(int blendMode) const;

	bool isAutomaticAlphaPreserve() const
	{
		return m_automaticAlphaPreserve || m_compatibilityMode;
	}

	static void setComboIndexBlocked(QComboBox *combo, int i);
	static void setButtonCheckedBlocked(QAbstractButton *button, bool checked);

	const Type m_type;
	QComboBox *const m_blendModeCombo;
	QComboBox *const m_eraseModeCombo;
	QAbstractButton *const m_alphaPreserveButton;
	QAbstractButton *const m_eraseModeButton;
	QVector<int> m_history;
	bool m_automaticAlphaPreserve = true;
	bool m_compatibilityMode = false;
	bool m_myPaint = false;
	bool m_eraseMode = false;
};

#endif
