// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PRESETSELECTORPLUGIN_H
#define PRESETSELECTORPLUGIN_H

#include <QtUiPlugin/QDesignerCustomWidgetCollectionInterface>

class PresetSelectorPlugin : public QObject, public QDesignerCustomWidgetInterface
{
Q_OBJECT
Q_INTERFACES(QDesignerCustomWidgetInterface)

public:
	PresetSelectorPlugin(QObject *parent=nullptr);

	bool isContainer() const;
	bool isInitialized() const;
	QIcon icon() const;
	QString domXml() const;
	QString group() const;
	QString includeFile() const;
	QString name() const;
	QString toolTip() const;
	QString whatsThis() const;
	QWidget *createWidget(QWidget *parent);
	void initialize(QDesignerFormEditorInterface *core);

private:
	bool initialized;
};

#endif

