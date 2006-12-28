#include <QtPlugin>

#include "../../imageselector.h"
#include "plugin.h"

ImageSelectorPlugin::ImageSelectorPlugin(QObject *parent)
	: QObject(parent)
{
	initialized = false;
}

void ImageSelectorPlugin::initialize(QDesignerFormEditorInterface * /* core */)
{
	if (initialized)
		return;

	initialized = true;
}

bool ImageSelectorPlugin::isInitialized() const
{
    return initialized;
}

QWidget *ImageSelectorPlugin::createWidget(QWidget *parent)
{
    return new ImageSelector(parent);
}

QString ImageSelectorPlugin::name() const
{
    return "ImageSelector";
}

QString ImageSelectorPlugin::group() const
{
    return "DrawPile Widgets";
}

QIcon ImageSelectorPlugin::icon() const
{
    return QIcon(":image.png");
}

QString ImageSelectorPlugin::toolTip() const
{
    return "Image selecting preview widget";
}

QString ImageSelectorPlugin::whatsThis() const
{
    return "";
}

bool ImageSelectorPlugin::isContainer() const
{
    return false;
}

QString ImageSelectorPlugin::domXml() const
{
    return "<widget class=\"ImageSelector\" name=\"imageSelector\">\n"
           " <property name=\"geometry\">\n"
           "  <rect>\n"
           "   <x>0</x>\n"
           "   <y>0</y>\n"
           "   <width>128</width>\n"
           "   <height>128</height>\n"
           "  </rect>\n"
           " </property>\n"
           "</widget>\n";
}

QString ImageSelectorPlugin::includeFile() const
{
    return "imageselector.h";
}

Q_EXPORT_PLUGIN2(imageselectorplugin, ImageSelectorPlugin)

