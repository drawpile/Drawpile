// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/blendmodes.h"
#include <QAbstractButton>
#include <QComboBox>
#include <QCoreApplication>
#include <QSignalBlocker>
#include <QStandardItemModel>


namespace {
static bool isMatchingBlendMode(QAbstractItemModel *model, int i, int mode)
{
	QModelIndex idx = model->index(i, 0);
	return idx.isValid() && model->flags(idx).testFlag(Qt::ItemIsEnabled) &&
		   model->data(idx, Qt::UserRole).toInt() == mode;
}
}

int searchBlendModeComboIndex(QComboBox *combo, int mode)
{
	QAbstractItemModel *model = combo ? combo->model() : nullptr;
	if(model) {
		int blendModeCount = combo->count();
		for(int i = 0; i < blendModeCount; ++i) {
			if(isMatchingBlendMode(model, i, mode)) {
				return i;
			}
		}

		int alphaPreservingMode = canvas::blendmode::toAlphaPreserving(mode);
		if(alphaPreservingMode != mode) {
			for(int i = 0; i < blendModeCount; ++i) {
				if(isMatchingBlendMode(model, i, alphaPreservingMode)) {
					return i;
				}
			}
		}

		int alphaAffectingMode = canvas::blendmode::toAlphaAffecting(mode);
		if(alphaAffectingMode != mode) {
			for(int i = 0; i < blendModeCount; ++i) {
				if(isMatchingBlendMode(model, i, alphaAffectingMode)) {
					return i;
				}
			}
		}
	}
	return -1;
}


namespace {

static constexpr int RECOLOR_ENABLED = 0;
static constexpr int RECOLOR_DISABLED = 1;
static constexpr int RECOLOR_OMITTED = 2;

static void addSeparator(QStandardItemModel *model)
{
	QStandardItem *separator = new QStandardItem();
	separator->setAccessibleDescription(QStringLiteral("separator"));
	separator->setData(-1, Qt::UserRole);
	separator->setFlags(
		separator->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
	model->appendRow(separator);
}

static void addBlendModesTo(
	QStandardItemModel *model, int recolor, bool compatibilityMode,
	bool myPaint, const QVector<canvas::blendmode::Named> &modes)
{
	for(int i = 0, count = modes.size(); i < count; ++i) {
		const canvas::blendmode::Named &m = modes[i];
		if(m.separatorAbove && i != 0) {
			addSeparator(model);
		}

		QStandardItem *item = new QStandardItem(m.name);
		item->setData(int(m.mode), Qt::UserRole);
		if(m.mode == DP_BLEND_MODE_RECOLOR) {
			if(recolor == RECOLOR_DISABLED) {
				item->setEnabled(false);
				model->appendRow(item);
			} else if(recolor != RECOLOR_OMITTED) {
				model->appendRow(item);
			}
		} else {
			model->appendRow(item);
		}

		if(compatibilityMode &&
		   !(myPaint ? m.myPaintCompatible : m.compatible)) {
			item->setEnabled(false);
		}

		if(m.separatorBelow && i != count - 1) {
			addSeparator(model);
		}
	}
}

static void addLayerBlendModesTo(
	QStandardItemModel *model, int recolor, bool compatibilityMode)
{
	addBlendModesTo(
		model, recolor, compatibilityMode, false,
		canvas::blendmode::layerModeNames());
}

static void addGroupBlendModesTo(
	QStandardItemModel *model, int recolor, bool compatibilityMode)
{
	QStandardItem *item = new QStandardItem(QCoreApplication::translate(
		"dialogs::LayerProperties", "Pass Through"));
	item->setData(-1, Qt::UserRole);
	model->appendRow(item);
	addLayerBlendModesTo(model, recolor, compatibilityMode);
}

static QStandardItemModel *layerBlendModes()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addLayerBlendModesTo(model, RECOLOR_ENABLED, false);
	}
	return model;
}

QStandardItemModel *layerBlendModesRecolorDisabled()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addLayerBlendModesTo(model, RECOLOR_DISABLED, false);
	}
	return model;
}

QStandardItemModel *layerBlendModesRecolorOmitted()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addLayerBlendModesTo(model, RECOLOR_OMITTED, false);
	}
	return model;
}

QStandardItemModel *layerBlendModesCompat()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addLayerBlendModesTo(model, RECOLOR_ENABLED, true);
	}
	return model;
}

QStandardItemModel *groupBlendModes()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addGroupBlendModesTo(model, RECOLOR_ENABLED, false);
	}
	return model;
}

QStandardItemModel *groupBlendModesRecolorDisabled()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addGroupBlendModesTo(model, RECOLOR_DISABLED, false);
	}
	return model;
}

QStandardItemModel *groupBlendModesRecolorOmitted()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addGroupBlendModesTo(model, RECOLOR_OMITTED, false);
	}
	return model;
}

QStandardItemModel *groupBlendModesCompat()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addGroupBlendModesTo(model, RECOLOR_ENABLED, true);
	}
	return model;
}

QStandardItemModel *brushBlendModes()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, false, false,
			canvas::blendmode::brushModeNames());
	}
	return model;
}

QStandardItemModel *brushBlendModesRecolorOmitted()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_OMITTED, false, false,
			canvas::blendmode::brushModeNames());
	}
	return model;
}

QStandardItemModel *brushBlendModesErase()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, false, false,
			canvas::blendmode::eraserModeNames());
	}
	return model;
}

QStandardItemModel *brushBlendModesCompat()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, true, false,
			canvas::blendmode::brushModeNames());
	}
	return model;
}

QStandardItemModel *brushBlendModesEraseCompat()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, true, false,
			canvas::blendmode::eraserModeNames());
	}
	return model;
}

QStandardItemModel *brushBlendModesMyPaintCompat()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, true, true,
			canvas::blendmode::brushModeNames());
	}
	return model;
}

QStandardItemModel *brushBlendModesEraseMyPaintCompat()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, true, true,
			canvas::blendmode::eraserModeNames());
	}
	return model;
}

QStandardItemModel *fillBlendModes()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, false, false,
			canvas::blendmode::pasteModeNames());
	}
	return model;
}

QStandardItemModel *fillBlendModesRecolorOmitted()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_OMITTED, false, false,
			canvas::blendmode::pasteModeNames());
	}
	return model;
}

QStandardItemModel *fillBlendModesCompat()
{
	static QStandardItemModel *model;
	if(!model) {
		model = new QStandardItemModel;
		addBlendModesTo(
			model, RECOLOR_ENABLED, true, false,
			canvas::blendmode::pasteModeNames());
	}
	return model;
}

}

QStandardItemModel *getLayerBlendModesFor(
	bool group, bool clip, bool automaticAlphaPreserve, bool compatibilityMode)
{
	if(compatibilityMode) {
		return group ? groupBlendModesCompat() : layerBlendModesCompat();
	} else if(automaticAlphaPreserve) {
		if(clip) {
			return group ? groupBlendModesRecolorDisabled()
						 : layerBlendModesRecolorDisabled();
		} else {
			return group ? groupBlendModes() : layerBlendModes();
		}
	} else {
		return group ? groupBlendModesRecolorOmitted()
					 : layerBlendModesRecolorOmitted();
	}
}

QStandardItemModel *getBrushBlendModesFor(
	bool erase, bool automaticAlphaPreserve, bool compatibilityMode,
	bool myPaint)
{
	if(compatibilityMode) {
		if(myPaint) {
			return erase ? brushBlendModesEraseMyPaintCompat()
						 : brushBlendModesMyPaintCompat();
		} else {
			return erase ? brushBlendModesEraseCompat()
						 : brushBlendModesCompat();
		}
	} else if(erase) {
		return brushBlendModesErase();
	} else if(automaticAlphaPreserve) {
		return brushBlendModes();
	} else {
		return brushBlendModesRecolorOmitted();
	}
}

QStandardItemModel *
getFillBlendModesFor(bool automaticAlphaPreserve, bool compatibilityMode)
{
	if(compatibilityMode) {
		return fillBlendModesCompat();
	} else if(automaticAlphaPreserve) {
		return fillBlendModes();
	} else {
		return fillBlendModesRecolorOmitted();
	}
}


BlendModeManager *BlendModeManager::initBrush(
	QComboBox *blendModeCombo, QComboBox *eraseModeCombo,
	QAbstractButton *alphaPreserveButton, QAbstractButton *eraseModeButton,
	QObject *parent)
{
	return new BlendModeManager(
		Type::Brush, blendModeCombo, eraseModeCombo, alphaPreserveButton,
		eraseModeButton, parent);
}

BlendModeManager *BlendModeManager::initFill(
	QComboBox *blendModeCombo, QAbstractButton *alphaPreserveButton,
	QObject *parent)
{
	return new BlendModeManager(
		Type::Fill, blendModeCombo, nullptr, alphaPreserveButton, nullptr,
		parent);
}

BlendModeManager::SignalBlocker::SignalBlocker(BlendModeManager *bmm)
	: m_blendModeManager(block(bmm))
	, m_blendModeCombo(block(bmm->m_blendModeCombo))
	, m_eraseModeCombo(block(bmm->m_eraseModeCombo))
	, m_alphaPreserveButton(block(bmm->m_alphaPreserveButton))
	, m_eraseModeButton(block(bmm->m_eraseModeButton))
{
}

BlendModeManager::SignalBlocker::~SignalBlocker()
{
	unblock(m_eraseModeButton);
	unblock(m_alphaPreserveButton);
	unblock(m_eraseModeCombo);
	unblock(m_blendModeCombo);
	unblock(m_blendModeManager);
}

QObject *BlendModeManager::SignalBlocker::block(QObject *object)
{
	if(object && !object->signalsBlocked()) {
		object->blockSignals(true);
		return object;
	} else {
		return nullptr;
	}
}

void BlendModeManager::SignalBlocker::unblock(QObject *object)
{
	if(object) {
		object->blockSignals(false);
	}
}

BlendModeManager::BlendModeManager(
	Type type, QComboBox *blendModeCombo, QComboBox *eraseModeCombo,
	QAbstractButton *alphaPreserveButton, QAbstractButton *eraseModeButton,
	QObject *parent)
	: QObject(parent)
	, m_type(type)
	, m_blendModeCombo(blendModeCombo)
	, m_eraseModeCombo(eraseModeCombo)
	, m_alphaPreserveButton(alphaPreserveButton)
	, m_eraseModeButton(eraseModeButton)
{
	initBlendModeOptions();
	connect(
		this, &BlendModeManager::blendModeChanged, this,
		&BlendModeManager::addToHistory);
	connect(
		m_blendModeCombo, QOverload<int>::of(&QComboBox::activated), this,
		&BlendModeManager::updateBlendModeIndex);
	connect(
		m_alphaPreserveButton, &QAbstractButton::clicked, this,
		&BlendModeManager::updateAlphaPreserve);
	if(m_eraseModeCombo) {
		connect(
			m_eraseModeCombo, QOverload<int>::of(&QComboBox::activated), this,
			&BlendModeManager::updateEraseModeIndex);
	}
	if(m_eraseModeButton) {
		connect(
			m_eraseModeButton, &QAbstractButton::clicked, this,
			&BlendModeManager::setEraseMode);
	}
}

void BlendModeManager::setAutomaticAlphaPerserve(bool automaticAlphaPreserve)
{
	if(automaticAlphaPreserve != m_automaticAlphaPreserve) {
		m_automaticAlphaPreserve = automaticAlphaPreserve;
		reinitBlendModeOptions();
	}
}

void BlendModeManager::setCompatibilityMode(bool compatibilityMode)
{
	if(compatibilityMode != m_compatibilityMode) {
		m_compatibilityMode = compatibilityMode;
		reinitBlendModeOptions();
	}
}

void BlendModeManager::setMyPaint(bool myPaint)
{
	if(myPaint != m_myPaint) {
		m_myPaint = myPaint;
		reinitBlendModeOptions();
	}
}

int BlendModeManager::getCurrentBlendMode() const
{
	QComboBox *combo = m_eraseMode ? m_eraseModeCombo : m_blendModeCombo;
	int blendMode = combo && combo->count() != 0 ? combo->currentData().toInt()
					: m_eraseMode				 ? int(DP_BLEND_MODE_ERASE)
												 : int(DP_BLEND_MODE_NORMAL);
	canvas::blendmode::adjustAlphaBehavior(blendMode, m_alphaPreserve);
	return blendMode;
}

bool BlendModeManager::selectBlendMode(int blendMode)
{
	if(m_compatibilityMode &&
	   !canvas::blendmode::isCompatible(blendMode, m_myPaint)) {
		int fallbackMode =
			int(canvas::blendmode::presentsAsEraser(blendMode)
					? DP_BLEND_MODE_NORMAL
					: DP_BLEND_MODE_ERASE);
		DP_BlendMode alphaAffectingMode, alphaPreservingMode;
		if(canvas::blendmode::alphaPreservePair(
			   blendMode, &alphaAffectingMode, &alphaPreservingMode)) {
			if(blendMode != int(alphaAffectingMode) &&
			   canvas::blendmode::isCompatible(
				   int(alphaAffectingMode), m_myPaint)) {
				fallbackMode = int(alphaAffectingMode);
			} else if(
				blendMode != int(alphaPreservingMode) &&
				canvas::blendmode::isCompatible(
					int(alphaPreservingMode), m_myPaint)) {
				fallbackMode = int(alphaPreservingMode);
			}
		}
		blendMode = fallbackMode;
	}

	int blendModeIndex = searchBlendModeComboIndex(m_blendModeCombo, blendMode);
	if(blendModeIndex != -1) {
		m_alphaPreserve =
			canvas::blendmode::presentsAsAlphaPreserving(blendMode);
		if(m_eraseMode) {
			m_eraseMode = false;
			m_eraseModeCombo->hide();
			m_blendModeCombo->show();
		}
		setComboIndexBlocked(m_blendModeCombo, blendModeIndex);
		setButtonCheckedBlocked(m_alphaPreserveButton, m_alphaPreserve);
		setButtonCheckedBlocked(m_eraseModeButton, false);
		emit blendModeChanged(blendMode, false);
		return true;
	}

	int eraseModeIndex = searchBlendModeComboIndex(m_eraseModeCombo, blendMode);
	if(eraseModeIndex != -1) {
		m_alphaPreserve =
			canvas::blendmode::presentsAsAlphaPreserving(blendMode);
		if(!m_eraseMode) {
			m_eraseMode = true;
			m_eraseModeCombo->hide();
			m_blendModeCombo->show();
		}
		setComboIndexBlocked(m_eraseModeCombo, eraseModeIndex);
		setButtonCheckedBlocked(m_alphaPreserveButton, m_alphaPreserve);
		setButtonCheckedBlocked(m_eraseModeButton, true);
		emit blendModeChanged(blendMode, true);
		return true;
	}

	switch(blendMode) {
	case DP_BLEND_MODE_NORMAL:
	case DP_BLEND_MODE_RECOLOR:
	case DP_BLEND_MODE_ERASE:
		return false;
	default:
		return selectBlendMode(int(
			canvas::blendmode::presentsAsEraser(blendMode) ? DP_BLEND_MODE_ERASE
			: canvas::blendmode::preservesAlpha(blendMode)
				? DP_BLEND_MODE_RECOLOR
				: DP_BLEND_MODE_NORMAL));
	}
}

void BlendModeManager::setEraseMode(bool eraseMode)
{
	if(eraseMode && !m_eraseMode) {
		m_eraseMode = true;
		m_blendModeCombo->hide();
		m_eraseModeCombo->show();
		emit blendModeChanged(getCurrentBlendMode(), true);
	} else if(!eraseMode && m_eraseMode) {
		m_eraseMode = false;
		m_eraseModeCombo->hide();
		m_blendModeCombo->show();
		emit blendModeChanged(getCurrentBlendMode(), false);
	}
}

void BlendModeManager::toggleAlphaPreserve()
{
	int blendMode = getCurrentBlendMode();
	return updateAlphaPreserveFromBlendMode(
		blendMode, !canvas::blendmode::presentsAsAlphaPreserving(blendMode));
}

void BlendModeManager::toggleEraserMode()
{
	bool eraser =
		m_eraseModeCombo
			? m_eraseMode
			: canvas::blendmode::presentsAsEraser(getCurrentBlendMode());
	int blendMode = searchHistory(
		[eraser](int mode) {
			return canvas::blendmode::presentsAsEraser(mode) != eraser;
		},
		int(eraser ? DP_BLEND_MODE_NORMAL : DP_BLEND_MODE_ERASE));
	if(!isAutomaticAlphaPreserve()) {
		canvas::blendmode::adjustAlphaBehavior(blendMode, m_alphaPreserve);
	}
	selectBlendMode(blendMode);
}

void BlendModeManager::toggleBlendMode(int blendMode)
{
	bool keepAlphaPreserve = blendMode != int(DP_BLEND_MODE_RECOLOR);
	if(isModeSelected(blendMode)) {
		if(m_eraseMode) {
			blendMode = int(DP_BLEND_MODE_ERASE);
		} else {
			blendMode = int(DP_BLEND_MODE_NORMAL);
		}
	}

	if(keepAlphaPreserve && !isAutomaticAlphaPreserve()) {
		canvas::blendmode::adjustAlphaBehavior(blendMode, m_alphaPreserve);
	}
	selectBlendMode(blendMode);
}

void BlendModeManager::initBlendModeOptions()
{
	SignalBlocker blocker(this);
	switch(m_type) {
	case Type::Brush:
		m_blendModeCombo->setModel(getBrushBlendModesFor(
			false, m_automaticAlphaPreserve, m_compatibilityMode, m_myPaint));
		m_eraseModeCombo->setModel(getBrushBlendModesFor(
			true, m_automaticAlphaPreserve, m_compatibilityMode, m_myPaint));
		break;
	case Type::Fill:
		m_blendModeCombo->setModel(getFillBlendModesFor(
			m_automaticAlphaPreserve, m_compatibilityMode));
		break;
	}
}

void BlendModeManager::reinitBlendModeOptions()
{
	int prevMode = getCurrentBlendMode();
	initBlendModeOptions();
	selectBlendMode(prevMode);
}

void BlendModeManager::updateBlendModeIndex(int index)
{
	int blendMode = m_blendModeCombo->itemData(index).toInt();
	if(!isAutomaticAlphaPreserve()) {
		canvas::blendmode::adjustAlphaBehavior(blendMode, m_alphaPreserve);
	}
	selectBlendMode(blendMode);
}

void BlendModeManager::updateAlphaPreserve(bool alphaPreserve)
{
	updateAlphaPreserveFromBlendMode(getCurrentBlendMode(), alphaPreserve);
}

void BlendModeManager::updateAlphaPreserveFromBlendMode(
	int blendMode, bool alphaPreserve)
{
	selectBlendMode(
		getAlphaPreserveAdjustedBlendMode(blendMode, alphaPreserve));
}

int BlendModeManager::getAlphaPreserveAdjustedBlendMode(
	int blendMode, bool alphaPreserve) const
{
	// Automatic mode tries harder to do something useful in response to user
	// input, since some kinds of alpha preserve toggling are useless.
	if(m_automaticAlphaPreserve) {
		switch(blendMode) {
		case DP_BLEND_MODE_ERASE:
		case DP_BLEND_MODE_ERASE_PRESERVE:
		case DP_BLEND_MODE_COLOR_ERASE:
		case DP_BLEND_MODE_COLOR_ERASE_PRESERVE:
		case DP_BLEND_MODE_BEHIND:
		case DP_BLEND_MODE_BEHIND_PRESERVE:
			if(alphaPreserve) {
				// These modes can't preserve alpha, since that contradicts
				// their functionality. Switch to Recolor instead then.
				return DP_BLEND_MODE_RECOLOR;
			}
			break;
		case DP_BLEND_MODE_RECOLOR:
			if(!alphaPreserve) {
				// Return to Behind mode if that's the mode that we just came
				// from, so that the user can toggle between Behind and Recolor
				// sensibly without getting reverted to Normal. The erase modes
				// above can already be toggled to by enabling erase mode.
				return searchHistoryLimit(
					[](int mode) {
						return mode == DP_BLEND_MODE_BEHIND;
					},
					2, int(DP_BLEND_MODE_NORMAL));
			}
		}
	}

	canvas::blendmode::adjustAlphaBehavior(blendMode, alphaPreserve);

	if(m_compatibilityMode &&
	   !canvas::blendmode::isCompatible(blendMode, m_myPaint)) {
		return int(
			m_eraseMode		? DP_BLEND_MODE_ERASE
			: alphaPreserve ? DP_BLEND_MODE_RECOLOR
							: DP_BLEND_MODE_NORMAL);
	}

	return blendMode;
}

void BlendModeManager::updateEraseModeIndex(int index)
{
	int blendMode = m_eraseModeCombo->itemData(index).toInt();
	if(!isAutomaticAlphaPreserve()) {
		canvas::blendmode::adjustAlphaBehavior(blendMode, m_alphaPreserve);
	}
	selectBlendMode(blendMode);
}

void BlendModeManager::addToHistory(int blendMode)
{
	m_history.removeOne(blendMode);
	m_history.append(blendMode);
}

int BlendModeManager::searchHistory(
	const std::function<bool(int)> &predicate, int fallback) const
{
	return searchHistoryLimit(predicate, int(m_history.size()), fallback);
}

int BlendModeManager::searchHistoryLimit(
	const std::function<bool(int)> &predicate, int limit, int fallback) const
{
	int count = int(m_history.size());
	int effectiveCount = qMin(count, limit);
	for(int i = 0; i < effectiveCount; ++i) {
		int blendMode = m_history[count - i - 1];
		if(predicate(blendMode)) {
			return blendMode;
		}
	}
	return fallback;
}

bool BlendModeManager::isModeSelected(int blendMode) const
{
	int currentMode = getCurrentBlendMode();
	if(blendMode == currentMode) {
		return true;
	}

	DP_BlendMode alphaAffectingMode, alphaPreservingMode;
	if(blendMode != int(DP_BLEND_MODE_NORMAL) &&
	   blendMode != int(DP_BLEND_MODE_RECOLOR) &&
	   canvas::blendmode::alphaPreservePair(
		   currentMode, &alphaAffectingMode, &alphaPreservingMode)) {
		return blendMode == int(alphaAffectingMode) ||
			   blendMode == int(alphaPreservingMode);
	}

	return false;
}

void BlendModeManager::setComboIndexBlocked(QComboBox *combo, int i)
{
	if(combo) {
		QSignalBlocker blocker(combo);
		combo->setCurrentIndex(i);
	}
}

void BlendModeManager::setButtonCheckedBlocked(
	QAbstractButton *button, bool checked)
{
	if(button) {
		QSignalBlocker blocker(button);
		button->setChecked(checked);
	}
}
