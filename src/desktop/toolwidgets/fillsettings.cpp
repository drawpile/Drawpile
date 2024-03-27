// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/toolwidgets/fillsettings.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/tools/floodfill.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/toolproperties.h"
#include "ui_fillsettings.h"
#include <QButtonGroup>
#include <QIcon>

namespace tools {

namespace props {
static const ToolProperties::RangedValue<int> expand{
	QStringLiteral("expand"), 0, 0, 100},
	featherRadius{QStringLiteral("featherRadius"), 0, 0, 40},
	mode{QStringLiteral("mode"), 0, 0, 2},
	size{QStringLiteral("size"), 500, 10, 9999},
	gap{QStringLiteral("gap"), 0, 0, 32},
	source{QStringLiteral("source"), 2, 0, 2};
static const ToolProperties::RangedValue<double> tolerance{
	QStringLiteral("tolerance"), 0.0, 0.0, 1.0};
}

class FillSettings::FillLayerModel final : public QAbstractItemModel {
public:
	static constexpr int CURRENT_LAYER_ROW = 0;
	static constexpr int PREFIX_ROWS = 1;

	FillLayerModel(QObject *parent)
		: QAbstractItemModel{parent}
		, m_activeLayer{0}
		, m_source{int(FillSettings::Source::Layer)}
		, m_layers{}
		, m_layerIcon(QIcon::fromTheme("layer-visible-on"))
		, m_groupIcon(QIcon::fromTheme("folder"))
	{
	}

	QModelIndex index(
		int row, int column,
		const QModelIndex &parent = QModelIndex()) const override
	{
		if(parent.isValid() || column != 0) {
			return QModelIndex{};
		} else if(row == CURRENT_LAYER_ROW) {
			return createIndex(row, column, quintptr(0));
		} else {
			const Layer *layer = layerAt(row);
			return layer ? createIndex(row, column, quintptr(layer->id))
						 : QModelIndex{};
		}
	}

	QModelIndex parent(const QModelIndex &) const override
	{
		return QModelIndex{};
	}

	int rowCount(const QModelIndex &parent = QModelIndex()) const override
	{
		return parent.isValid() ? 0 : PREFIX_ROWS + m_layers.size();
	}

	int columnCount(const QModelIndex &parent = QModelIndex()) const override
	{
		return parent.isValid() ? 0 : 1;
	}

	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override
	{
		if(!index.isValid() || index.parent().isValid() ||
		   index.column() != 0) {
			return QVariant{};
		}

		switch(role) {
		case Qt::DisplayRole:
		case Qt::ToolTipRole:
			return getDisplayText(index);
		case Qt::DecorationRole:
			return getDecoration(index);
		case Qt::UserRole:
			return getLayerId(index);
		default:
			return QVariant{};
		}
	}

	void setActiveLayer(int layerId) { m_activeLayer = layerId; }

	void setSource(int source)
	{
		m_source = source;
		dataChanged(index(0, 0), index(columnCount() - 1, rowCount() - 1));
	}

	void setLayers(const QVector<canvas::LayerListItem> &items)
	{
		beginResetModel();
		m_layers.clear();
		int i = 0;
		while(i < items.size()) {
			const canvas::LayerListItem &item = items[i++];
			m_layers.append({item.id, item.title, item.group});
			addLayersRecursive(items, i, item, 1);
		}
		endResetModel();
	}

	int searchRowByLayerId(int layerId)
	{
		int count = m_layers.size();
		for(int i = 0; i < count; ++i) {
			if(m_layers[i].id == layerId) {
				return PREFIX_ROWS + i;
			}
		}
		return CURRENT_LAYER_ROW;
	}

private:
	struct Layer {
		int id;
		QString displayText;
		bool group;
	};

	QString getDisplayText(const QModelIndex &index) const
	{
		switch(m_source) {
		case int(FillSettings::Source::Merged):
			return FillSettings::tr("Merged Image");
		case int(FillSettings::Source::MergedWithoutBackground):
			return FillSettings::tr("Merged Without Background");
		default:
			int row = index.row();
			if(row == CURRENT_LAYER_ROW) {
				return FillSettings::tr("Current Layer");
			} else {
				const Layer *layer = layerAt(row);
				return layer ? layer->displayText : QString{};
			}
		}
	}

	int getLayerId(const QModelIndex &index) const
	{
		switch(index.row()) {
		case CURRENT_LAYER_ROW:
			return m_activeLayer;
		default:
			return int(index.internalId());
		}
	}

	QVariant getDecoration(const QModelIndex &index) const
	{
		if(m_source == int(FillSettings::Source::Layer)) {
			const Layer *layer = layerAt(index.row());
			if(layer) {
				return layer->group ? m_groupIcon : m_layerIcon;
			}
		}
		return QVariant();
	}

	void addLayersRecursive(
		const QVector<canvas::LayerListItem> &items, int &i,
		const canvas::LayerListItem &parent, int nesting)
	{
		int children = parent.children;
		for(int j = 0; j < children && i < items.size(); ++j) {
			const canvas::LayerListItem &item = items[i++];
			m_layers.append({item.id, item.title, item.group});
			addLayersRecursive(items, i, item, nesting + 1);
		}
	}

	const Layer *layerAt(int row) const
	{
		int i = row - PREFIX_ROWS;
		return i >= 0 && i < m_layers.size() ? &m_layers[i] : nullptr;
	}

	int m_activeLayer;
	int m_source;
	QVector<Layer> m_layers;
	QIcon m_layerIcon;
	QIcon m_groupIcon;
};


FillSettings::FillSettings(ToolController *ctrl, QObject *parent)
	: ToolSettings(ctrl, parent)
	, m_fillLayerModel(new FillLayerModel(this))
{
}

FillSettings::~FillSettings()
{
	delete m_ui;
}

QWidget *FillSettings::createUiWidget(QWidget *parent)
{
	QWidget *uiwidget = new QWidget(parent);
	m_ui = new Ui_FillSettings;
	m_ui->setupUi(uiwidget);
	m_ui->layer->setModel(m_fillLayerModel);
	m_ui->size->setExponentRatio(3.0);
	utils::setWidgetRetainSizeWhenHidden(m_ui->layer, true);

	m_sourceGroup = new QButtonGroup(this);
	m_sourceGroup->setExclusive(true);
	m_sourceGroup->addButton(m_ui->sourceMerged, int(Source::Merged));
	m_sourceGroup->addButton(
		m_ui->sourceMergedWithoutBackground,
		int(Source::MergedWithoutBackground));
	m_sourceGroup->addButton(m_ui->sourceLayer, int(Source::Layer));

	connect(
		m_ui->size, QOverload<int>::of(&QSpinBox::valueChanged), this,
		[this](int size) {
			emit pixelSizeChanged(size * 2 + 1);
		});
	connect(
		m_ui->tolerance, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::pushSettings);
	connect(
		m_ui->size, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::pushSettings);
	connect(
		m_ui->expand, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::pushSettings);
	connect(
		m_ui->feather, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::pushSettings);
	connect(
		m_ui->gap, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&FillSettings::pushSettings);
	connect(
		m_ui->layer, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		[this]() {
			m_ui->layer->setToolTip(m_ui->layer->currentText());
			pushSettings();
		});
	connect(
		m_ui->mode, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		&FillSettings::pushSettings);
	connect(
		m_sourceGroup,
		QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
		[this]() {
			updateLayerCombo(m_sourceGroup->checkedId());
			pushSettings();
		});
	return uiwidget;
}

void FillSettings::pushSettings()
{
	FloodFill *tool =
		static_cast<FloodFill *>(controller()->getTool(Tool::FLOODFILL));
	tool->setTolerance(
		m_ui->tolerance->value() / qreal(m_ui->tolerance->maximum()));
	tool->setExpansion(m_ui->expand->value());
	tool->setFeatherRadius(m_ui->feather->value());
	tool->setSize(m_ui->size->value());
	tool->setGap(m_ui->gap->value());

	switch(m_sourceGroup->checkedId()) {
	case int(Source::Merged):
		tool->setLayerId(0);
		break;
	case int(Source::MergedWithoutBackground):
		tool->setLayerId(-1);
		break;
	case int(Source::Layer):
		tool->setLayerId(m_ui->layer->currentData().toInt());
		break;
	}

	const auto mode = static_cast<Mode>(m_ui->mode->currentIndex());
	tool->setBlendMode(modeIndexToBlendMode(mode));
	if(mode != Erase) {
		m_previousMode = mode;
	}
}

void FillSettings::toggleEraserMode()
{
	int mode = m_ui->mode->currentIndex();
	m_ui->mode->setCurrentIndex(mode == Erase ? m_previousMode : Erase);
}

ToolProperties FillSettings::saveToolSettings()
{
	ToolProperties cfg(toolType());
	cfg.setValue(
		props::tolerance,
		m_ui->tolerance->value() / qreal(m_ui->tolerance->maximum()));
	cfg.setValue(props::expand, m_ui->expand->value());
	cfg.setValue(props::featherRadius, m_ui->feather->value());
	cfg.setValue(props::size, m_ui->size->value());
	cfg.setValue(props::gap, m_ui->gap->value());
	cfg.setValue(props::mode, m_ui->mode->currentIndex());
	cfg.setValue(props::source, m_sourceGroup->checkedId());
	return cfg;
}

void FillSettings::setActiveLayer(int layerId)
{
	m_fillLayerModel->setActiveLayer(layerId);
	if(m_ui->layer->currentIndex() == FillLayerModel::CURRENT_LAYER_ROW) {
		pushSettings();
	}
}

void FillSettings::setSourceLayerId(int layerId)
{
	int row = m_fillLayerModel->searchRowByLayerId(layerId);
	if(row >= FillLayerModel::PREFIX_ROWS) {
		m_ui->layer->setCurrentIndex(row);
		m_ui->sourceLayer->click();
	}
}

void FillSettings::setForeground(const QColor &color)
{
	brushes::ActiveBrush b;
	b.classic().size.max = 1;
	b.setQColor(color);
	controller()->setActiveBrush(b);
}

int FillSettings::getSize() const
{
	return m_ui->size->value() * 2 + 1;
}

void FillSettings::restoreToolSettings(const ToolProperties &cfg)
{
	m_ui->tolerance->setValue(
		cfg.value(props::tolerance) * m_ui->tolerance->maximum());
	m_ui->expand->setValue(cfg.value(props::expand));
	m_ui->feather->setValue(cfg.value(props::featherRadius));
	m_ui->size->setValue(cfg.value(props::size));
	m_ui->gap->setValue(cfg.value(props::gap));
	m_ui->mode->setCurrentIndex(cfg.value(props::mode));

	int source = cfg.value(props::source);
	updateLayerCombo(source);
	QAbstractButton *sourceButton = m_sourceGroup->button(source);
	if(sourceButton) {
		sourceButton->setChecked(true);
	}

	pushSettings();
}

void FillSettings::setLayerList(canvas::LayerListModel *layerlist)
{
	connect(
		layerlist, &canvas::LayerListModel::layersChanged, this,
		&FillSettings::setLayers);
	setLayers(layerlist->layerItems());
}

void FillSettings::setLayers(const QVector<canvas::LayerListItem> &items)
{
	int index = m_ui->layer->currentIndex();
	int layerId = m_ui->layer->currentData().toInt();
	m_fillLayerModel->setLayers(items);
	if(index >= FillLayerModel::PREFIX_ROWS) {
		index = m_fillLayerModel->searchRowByLayerId(layerId);
	}
	m_ui->layer->setCurrentIndex(index);
}

int FillSettings::modeIndexToBlendMode(int mode)
{
	switch(mode) {
	case Behind:
		return DP_BLEND_MODE_BEHIND;
	case Erase:
		return DP_BLEND_MODE_ERASE;
	default:
		return DP_BLEND_MODE_NORMAL;
	}
}

void FillSettings::quickAdjust1(qreal adjustment)
{
	m_quickAdjust1 += adjustment * 10.0;
	qreal i;
	qreal f = modf(m_quickAdjust1, &i);
	if(int(i)) {
		m_quickAdjust1 = f;
		m_ui->size->setValue(m_ui->size->value() + i);
	}
}

void FillSettings::stepAdjust1(bool increase)
{
	KisSliderSpinBox *size = m_ui->size;
	size->setValue(stepLogarithmic(
		size->minimum(), size->maximum(), size->value(), increase));
}

void FillSettings::updateLayerCombo(int source)
{
	m_ui->layer->setEnabled(true);
	m_fillLayerModel->setSource(source);
	m_ui->layer->setToolTip(m_ui->layer->currentText());
	m_ui->layer->setEnabled(source == int(Source::Layer));
}

}
