// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LAYERPROPERTIES_H
#define LAYERPROPERTIES_H

#include "libclient/canvas/layerlist.h"

#include <QDialog>

class QComboBox;
class Ui_LayerProperties;

namespace drawdance {
    class Message;
}

namespace dialogs {

class LayerProperties final : public QDialog
{
Q_OBJECT
public:
	explicit LayerProperties(uint8_t localUser, QWidget *parent = nullptr);
	~LayerProperties() override;

	void setLayerItem(const canvas::LayerListItem &data, const QString &creator, bool isDefault);
	void setControlsEnabled(bool enabled);
	void setOpControlsEnabled(bool enabled);
    void setCompatibilityMode(bool compatibilityMode);

	int layerId() const { return m_item.id; }

    static void initBlendModeCombo(QComboBox *combo);
    static void setBlendModeComboCompatibilityMode(
        QComboBox *combo, bool compatibilityMode);

signals:
	void layerCommands(int count, const drawdance::Message *msgs);
	void visibilityChanged(int layerId, bool visible);

protected:
    virtual void showEvent(QShowEvent *event) override;

private slots:
	void emitChanges();

private:
	int searchBlendModeIndex(DP_BlendMode mode);

    Ui_LayerProperties *m_ui;
	canvas::LayerListItem m_item;
	bool m_wasDefault;
	uint8_t m_user;
};

}

#endif
