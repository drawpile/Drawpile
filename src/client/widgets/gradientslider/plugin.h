#ifndef GRADIENTSLIDERPLUGIN_H
#define GRADIENTSLIDERPLUGIN_H

#include <QDesignerCustomWidgetInterface>

class GradientSliderPlugin : public QObject, public QDesignerCustomWidgetInterface
{
Q_OBJECT
Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QDesignerCustomWidgetInterface" FILE "gradientslider.json")
Q_INTERFACES(QDesignerCustomWidgetInterface)

public:
	GradientSliderPlugin(QObject *parent = 0);

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

