// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LAYERPROPERTIES_H
#define LAYERPROPERTIES_H
#include "desktop/utils/qtguicompat.h"
#include "libclient/canvas/layerlist.h"
#include <QDialog>
#include <QIcon>

class QComboBox;
class QStandardItemModel;
class Ui_LayerProperties;

namespace net {
class Message;
}

namespace dialogs {

class LayerProperties final : public QDialog {
	Q_OBJECT
public:
	explicit LayerProperties(uint8_t localUser, QWidget *parent = nullptr);
	~LayerProperties() override;

	void setNewLayerItem(int selectedId, bool group, const QString &title);
	void setLayerItem(
		const canvas::LayerListItem &data, const QString &creator,
		bool isDefault);
	void updateLayerItem(
		const canvas::LayerListItem &data, const QString &creator,
		bool isDefault);
	void setControlsEnabled(bool enabled);
	void setOpControlsEnabled(bool enabled);

	int layerId() const { return m_item.id; }

	static void updateBlendMode(
		QComboBox *combo, DP_BlendMode mode, bool group, bool isolated,
		bool clip, bool automaticAlphaPreserve);

	static QStandardItemModel *
	blendModesFor(bool group, bool clip, bool automaticAlphaPreserve);
	static QStandardItemModel *layerBlendModes();
	static QStandardItemModel *layerBlendModesRecolorDisabled();
	static QStandardItemModel *layerBlendModesRecolorOmitted();
	static QStandardItemModel *groupBlendModes();
	static QStandardItemModel *groupBlendModesRecolorDisabled();
	static QStandardItemModel *groupBlendModesRecolorOmitted();

signals:
	void addLayerOrGroupRequested(
		int selectedId, bool group, const QString &title, int opacityPercent,
		int blendMode, bool isolated, bool censored, bool clip,
		bool defaultLayer, bool visible, int sketchOpacityPercent,
		const QColor &sketchTint);
	void layerCommands(int count, const net::Message *msgs);
	void visibilityChanged(int layerId, bool visible);
	void sketchModeChanged(int layerId, int opacityPercent, const QColor &tint);

protected:
	virtual void showEvent(QShowEvent *event) override;

private:
	static constexpr int RECOLOR_ENABLED = 0;
	static constexpr int RECOLOR_DISABLED = 1;
	static constexpr int RECOLOR_OMITTED = 2;

	void setAutomaticAlphaPerserve(bool automaticAlphaPreserve);
	void updateAlpha(QAbstractButton *button);
	void updateAlphaBasedOnBlendMode(int index);
	void updateSketchMode(compat::CheckBoxState state);
	void showSketchTintColorPicker();
	void setSketchTintTo(const QColor &color);
	void setSketchParamsFromSettings();
	void saveSketchParametersToSettings(int opacityPercent, const QColor &tint);
	void apply();
	void emitChanges();
	static void addGroupBlendModesTo(QStandardItemModel *model, int recolor);
	static void addBlendModesTo(QStandardItemModel *model, int recolor);
	static int searchBlendModeIndex(QComboBox *combo, DP_BlendMode mode);

	Ui_LayerProperties *m_ui;
	canvas::LayerListItem m_item = canvas::LayerListItem::null();
	int m_selectedId = 0;
	bool m_updating = false;
	bool m_wasDefault = false;
	bool m_controlsEnabled = true;
	bool m_automaticAlphaPreserve = true;
	uint8_t m_user;
};

}

#endif
