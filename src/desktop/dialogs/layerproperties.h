// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LAYERPROPERTIES_H
#define LAYERPROPERTIES_H
#include "libclient/canvas/layerlist.h"
#include <QDialog>

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
	void setCompatibilityMode(bool compatibilityMode);

	int layerId() const { return m_item.id; }

	static void updateBlendMode(
		QComboBox *combo, DP_BlendMode mode, bool group, bool isolated,
		bool compatibilityMode);

	static QStandardItemModel *layerBlendModes();
	static QStandardItemModel *groupBlendModes();
	static QStandardItemModel *compatibilityLayerBlendModes();

signals:
	void addLayerOrGroupRequested(
		int selectedId, bool group, const QString &title, int opacityPercent,
		int blendMode, bool isolated, bool censored, bool defaultLayer,
		bool visible);
	void layerCommands(int count, const net::Message *msgs);
	void visibilityChanged(int layerId, bool visible);

protected:
	virtual void showEvent(QShowEvent *event) override;

private:
	void apply();
	void emitChanges();
	static void addBlendModesTo(QStandardItemModel *model);
	static int searchBlendModeIndex(QComboBox *combo, DP_BlendMode mode);

	Ui_LayerProperties *m_ui;
	canvas::LayerListItem m_item = canvas::LayerListItem::null();
	int m_selectedId = 0;
	bool m_wasDefault = false;
	bool m_compatibilityMode = false;
	bool m_controlsEnabled = true;
	uint8_t m_user;
};

}

#endif
