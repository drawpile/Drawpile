// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef FILMSTRIPPLUGIN_H
#define FILMSTRIPPLUGIN_H

#include <QtUiPlugin/QDesignerCustomWidgetCollectionInterface>

class FilmstripPlugin : public QObject, public QDesignerCustomWidgetInterface
{
Q_OBJECT
Q_INTERFACES(QDesignerCustomWidgetInterface)

public:
	FilmstripPlugin(QObject *parent = 0);

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

