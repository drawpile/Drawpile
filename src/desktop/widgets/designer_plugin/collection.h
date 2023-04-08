// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWPILE_PLUGIN_COLLECTION_H
#define DRAWPILE_PLUGIN_COLLECTION_H

#include <QtUiPlugin/QDesignerCustomWidgetCollectionInterface>

class DrawpileWidgetCollection : public QObject, public QDesignerCustomWidgetCollectionInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "drawpile.DrawpileWidgets")
    Q_INTERFACES(QDesignerCustomWidgetCollectionInterface)

public:
	explicit DrawpileWidgetCollection(QObject *parent=0);

	QList<QDesignerCustomWidgetInterface*> customWidgets() const;

private:
	QList<QDesignerCustomWidgetInterface*> widgets;
};

#endif

