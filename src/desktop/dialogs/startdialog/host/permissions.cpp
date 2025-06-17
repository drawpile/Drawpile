// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/host/permissions.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include "desktop/widgets/noscroll.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStylePainter>
#include <QVBoxLayout>
#include <functional>

using std::placeholders::_1;

namespace dialogs {
namespace startdialog {
namespace host {

namespace {

// Doesn't show icons on the combo box itself, since those are already on the
// buttons next to it, so having them in the combo box adds too much noise.
class PermissionComboBox : public widgets::NoScrollComboBox {
public:
	explicit PermissionComboBox(QWidget *parent = nullptr)
		: widgets::NoScrollComboBox(parent)
	{
	}

protected:
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	// SPDX-SnippetBegin
	// SPDX-License-Identifier: GPL-3.0-only
	// SDPX—SnippetName: combo box painting to remove icons
	void paintEvent(QPaintEvent *) override
	{
		QStylePainter painter(this);
		painter.setPen(palette().color(QPalette::Text));

		// draw the combobox frame, focusrect and selected etc.
		QStyleOptionComboBox opt;
		initStyleOption(&opt);
		opt.currentIcon = QIcon(); // Drawpile patch.
		opt.iconSize = QSize();	   // Drawpile patch.
		painter.drawComplexControl(QStyle::CC_ComboBox, opt);

		if(currentIndex() < 0 && !placeholderText().isEmpty()) {
			opt.palette.setBrush(
				QPalette::ButtonText, opt.palette.placeholderText());
			opt.currentText = placeholderText();
		}

		// draw the icon and text
		painter.drawControl(QStyle::CE_ComboBoxLabel, opt);
	}
	// SPDX-SnippetEnd
#else
	void initStyleOption(QStyleOptionComboBox *option) const override
	{
		QComboBox::initStyleOption(option);
		option->currentIcon = QIcon();
		option->iconSize = QSize();
	}
#endif
};

class PermissionSliderSpinBox : public KisSliderSpinBox {
public:
	explicit PermissionSliderSpinBox(QWidget *parent = nullptr)
		: KisSliderSpinBox(parent)
	{
	}

protected:
	void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};

}

Permissions::Permissions(QWidget *parent)
	: QWidget(parent)
{
	setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	QScrollArea *permissionScroll = new QScrollArea;
	permissionScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	permissionScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	utils::bindKineticScrolling(permissionScroll);
	layout->addWidget(permissionScroll, 1);

	QWidget *permissionsWidget = new widgets::NoScrollWidget;
	permissionScroll->setWidgetResizable(true);
	permissionScroll->setWidget(permissionsWidget);

	QVBoxLayout *permissionsLayout = new QVBoxLayout(permissionsWidget);

	m_undoLimitSpinner = new PermissionSliderSpinBox;
	m_undoLimitSpinner->setPrefix(tr("Undo Limit: "));
	m_undoLimitSpinner->setRange(
		DP_CANVAS_HISTORY_UNDO_DEPTH_MIN, DP_CANVAS_HISTORY_UNDO_DEPTH_MAX);
	permissionsLayout->addWidget(m_undoLimitSpinner);

	QGridLayout *featureGrid = new QGridLayout;
	featureGrid->setContentsMargins(0, 0, 0, 0);
	featureGrid->setColumnStretch(3, 1);
	permissionsLayout->addLayout(featureGrid);

	int row = 0;
	desktop::settings::Settings &settings = dpApp().settings();
	QVariantMap lastPermissions = settings.lastSessionPermissions();
	addPerm(
		featureGrid, row, lastPermissions, m_permPutImage,
		int(DP_FEATURE_PUT_IMAGE), QIcon::fromTheme("fill-color"),
		tr("Cut, paste and fill:"));
	addPerm(
		featureGrid, row, lastPermissions, m_permRegionMove,
		int(DP_FEATURE_REGION_MOVE), QIcon::fromTheme("drawpile_transform"),
		tr("Transform selections:"));
	addPerm(
		featureGrid, row, lastPermissions, m_permCreateAnnotation,
		int(DP_FEATURE_CREATE_ANNOTATION), QIcon::fromTheme("draw-text"),
		tr("Create annotations:"));
	addPerm(
		featureGrid, row, lastPermissions, m_permOwnLayers,
		int(DP_FEATURE_OWN_LAYERS), QIcon::fromTheme("layer-visible-on"),
		tr("Manage own layers:"));
	addPerm(
		featureGrid, row, lastPermissions, m_permEditLayers,
		int(DP_FEATURE_EDIT_LAYERS), QIcon::fromTheme("configure"),
		tr("Manage all layers:"));
	addPerm(
		featureGrid, row, lastPermissions, m_permResize, int(DP_FEATURE_RESIZE),
		QIcon::fromTheme("zoom-select"), tr("Change canvas size:"));
	addPerm(
		featureGrid, row, lastPermissions, m_permBackground,
		int(DP_FEATURE_BACKGROUND), QIcon::fromTheme("edit-image"),
		tr("Change background:"));
	addPerm(
		featureGrid, row, lastPermissions, m_permMypaint,
		int(DP_FEATURE_MYPAINT), QIcon::fromTheme("drawpile_mypaint"),
		tr("MyPaint brushes:"));
	addPerm(
		featureGrid, row, lastPermissions, m_permPigment,
		int(DP_FEATURE_PIGMENT), QIcon::fromTheme("draw-brush"),
		tr("Pigment brushes:"));
	addPerm(
		featureGrid, row, lastPermissions, m_permLaser, int(DP_FEATURE_LASER),
		QIcon::fromTheme("cursor-arrow"), tr("Laser pointer:"));
	addPerm(
		featureGrid, row, lastPermissions, m_permTimeline,
		int(DP_FEATURE_TIMELINE), QIcon::fromTheme("keyframe"),
		tr("Animation timeline:"));
	addPerm(
		featureGrid, row, lastPermissions, m_permUndo, int(DP_FEATURE_UNDO),
		QIcon::fromTheme("edit-undo"), tr("Undo and redo:"));
	addPerm(
		featureGrid, row, lastPermissions, m_permKickBan, -1,
		QIcon::fromTheme("drawpile_ban"), tr("Kick and ban:"));

	QGridLayout *limitGrid = new QGridLayout;
	limitGrid->setContentsMargins(0, 0, 0, 0);
	limitGrid->setColumnStretch(2, 1);
	permissionsLayout->addLayout(limitGrid);

	row = 0;
	addLimit(
		limitGrid, row, lastPermissions, m_limitBrushSize,
		DP_FEATURE_LIMIT_BRUSH_SIZE, QIcon::fromTheme("drawpile_round"),
		tr("Maximum brush size:"));
	addLimit(
		limitGrid, row, lastPermissions, m_limitLayerCount,
		DP_FEATURE_LIMIT_LAYER_COUNT, QIcon::fromTheme("layer-visible-on"),
		tr("Maximum amount of layers:"));

	permissionsLayout->addStretch();

	settings.bindLastSessionUndoDepth(m_undoLimitSpinner);
}

void Permissions::reset()
{
	desktop::settings::Settings &settings = dpApp().settings();
	settings.setLastSessionUndoDepth(SESSION_UNDO_LIMIT_DEFAULT);
	settings.setLastSessionPermissions(QVariantMap());

	for(QHash<int, Perm *>::key_value_iterator it = m_permsMap.keyValueBegin(),
											   end = m_permsMap.keyValueEnd();
		it != end; ++it) {
		int feature = it->first;
		Perm *perm = it->second;
		perm->setTier(getDefaultFeatureTier(feature));
	}

	for(QHash<int, Limit *>::key_value_iterator
			it = m_limitsMap.keyValueBegin(),
			end = m_limitsMap.keyValueEnd();
		it != end; ++it) {
		int featureLimit = it->first;
		Limit *limit = it->second;
		for(int tier = TIER_COUNT - 1; tier >= 0; --tier) {
			limit->sliders[tier]->setValue(getDefaultLimit(featureLimit, tier));
		}
	}
}

void Permissions::load(const QJsonObject &json)
{
	desktop::settings::Settings &settings = dpApp().settings();
	int undoLimit = json[QStringLiteral("undolimit")].toInt(-1);
	if(undoLimit >= m_undoLimitSpinner->minimum()) {
		settings.setLastSessionUndoDepth(undoLimit);
	}
	settings.setLastSessionPermissions(QVariantMap());

	for(QHash<int, Perm *>::key_value_iterator it = m_permsMap.keyValueBegin(),
											   end = m_permsMap.keyValueEnd();
		it != end; ++it) {
		int feature = it->first;
		Perm *perm = it->second;
		int tier = json[getFeatureName(feature)].toInt(-1);
		if(tier >= 0) {
			perm->setTier(tier);
		}
	}

	for(QHash<int, Limit *>::key_value_iterator
			it = m_limitsMap.keyValueBegin(),
			end = m_limitsMap.keyValueEnd();
		it != end; ++it) {
		int featureLimit = it->first;
		Limit *limit = it->second;
		QJsonObject limitJson =
			json.value(getFeatureLimitName(featureLimit)).toObject();
		for(int tier = TIER_COUNT - 1; tier >= 0; --tier) {
			QJsonValue jsonValue = limitJson.value(getAccessTierName(tier));
			if(jsonValue.isDouble()) {
				int value = jsonValue.toInt(-1);
				limit->sliders[tier]->setValue(value);
			}
		}
	}
}

QJsonObject Permissions::save() const
{
	QJsonObject json;
	json.insert(QStringLiteral("undolimit"), m_undoLimitSpinner->value());

	for(QHash<int, Perm *>::const_key_value_iterator
			it = m_permsMap.constKeyValueBegin(),
			end = m_permsMap.constKeyValueEnd();
		it != end; ++it) {
		int feature = it->first;
		const Perm *perm = it->second;
		json.insert(getFeatureName(feature), perm->tier());
	}

	for(QHash<int, Limit *>::const_key_value_iterator
			it = m_limitsMap.constKeyValueBegin(),
			end = m_limitsMap.constKeyValueEnd();
		it != end; ++it) {
		int featureLimit = it->first;
		const Limit *limit = it->second;
		QJsonObject limitJson;
		for(int tier = 0; tier < TIER_COUNT; ++tier) {
			limitJson.insert(
				getAccessTierName(tier), limit->sliders[tier]->value());
		}
		json.insert(getFeatureLimitName(featureLimit), limitJson);
	}

	return json;
}

void Permissions::host(
	int &outUndoLimit, QHash<int, int> &outFeaturePermissions,
	bool &outDeputies, QHash<int, QHash<int, int>> &outFeatureLimits)
{
	outUndoLimit = m_undoLimitSpinner->value();

	for(QHash<int, Perm *>::const_key_value_iterator
			it = m_permsMap.constKeyValueBegin(),
			end = m_permsMap.constKeyValueEnd();
		it != end; ++it) {
		int feature = it->first;
		if(feature >= 0 && feature < DP_FEATURE_COUNT) {
			outFeaturePermissions.insert(feature, it->second->tier());
		}
	}
	outDeputies = m_permKickBan.tier() != DP_ACCESS_TIER_OPERATOR;

	for(QHash<int, Limit *>::const_key_value_iterator
			it = m_limitsMap.constKeyValueBegin(),
			end = m_limitsMap.constKeyValueEnd();
		it != end; ++it) {
		int featureLimit = it->first;
		const Limit *limit = it->second;
		QHash<int, int> tiers;
		for(int tier = 0; tier < TIER_COUNT; ++tier) {
			tiers.insert(tier, limit->sliders[tier]->value());
		}
		outFeatureLimits.insert(featureLimit, tiers);
	}
}

int Permissions::Perm::tier() const
{
	return combo->currentData().toInt();
}

void Permissions::Perm::setTier(int tier)
{
	QAbstractButton *featureButton = buttons->button(tier);
	if(featureButton) {
		featureButton->click();
	}
}

void Permissions::addLimit(
	QGridLayout *grid, int &row, const QVariantMap &lastPermissions,
	Limit &limit, int featureLimit, const QIcon &icon, const QString &text)
{
	static_assert(TIER_COUNT == DP_ACCESS_TIER_COUNT);
	m_limitsMap.insert(featureLimit, &limit);

	limit.icon = makeIconLabel(icon);
	limit.label = new QLabel(text);
	grid->addWidget(limit.icon, row, 0);
	grid->addWidget(limit.label, row, 1, 1, 2);
	++row;

	int limitMin = getLimitMinimum(featureLimit);
	int limitMax = getLimitMaximum(featureLimit);
	for(int tier = TIER_COUNT - 1; tier >= 0; --tier) {
		QToolButton *tierButton = new QToolButton;
		tierButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
		tierButton->setAutoRaise(true);
		tierButton->setEnabled(false);
		tierButton->setIcon(getTierIcon(tier));

		KisSliderSpinBox *tierSlider = new PermissionSliderSpinBox;
		//: %1 is an acccess tier, like "Operator" or "Everyone". Unless your
		//: language uses something other than a colon, leave this as it is.
		tierSlider->setPrefix(tr("%1: ").arg(getTierText(tier)));
		tierSlider->setSuffix(getLimitSuffix(featureLimit));
		tierSlider->setBlockUpdateSignalOnDrag(true);
		tierSlider->setRange(limitMin, limitMax);
		tierSlider->setExponentRatio(getLimitExponentRatio(featureLimit));
		limit.sliders[tier] = tierSlider;

		grid->addWidget(tierButton, row, 1);
		grid->addWidget(tierSlider, row, 2);
		++row;
	}

	for(int tier = TIER_COUNT - 1; tier >= 0; --tier) {
		bool ok;
		int value =
			lastPermissions.value(getFeatureLimitKey(featureLimit, tier))
				.toInt(&ok);

		KisSliderSpinBox *tierSlider = limit.sliders[tier];
		tierSlider->setValue(ok ? value : getDefaultLimit(featureLimit, tier));
		updateFeatureLimit(featureLimit, tier, tierSlider->value());

		connect(
			tierSlider, QOverload<int>::of(&KisSliderSpinBox::valueChanged),
			this, [this, featureLimit, tier](int newValue) {
				updateFeatureLimit(featureLimit, tier, newValue);
				persist();
			});
	}
}

void Permissions::addPerm(
	QGridLayout *grid, int &row, const QVariantMap &lastPermissions, Perm &perm,
	int feature, const QIcon &icon, const QString &text)
{
	m_permsMap.insert(feature, &perm);
	perm.icon = makeIconLabel(icon);
	perm.label = new QLabel(text);
	perm.buttons = new QButtonGroup(this);
	perm.combo = new PermissionComboBox;

	QHBoxLayout *buttonsLayout = new QHBoxLayout;
	buttonsLayout->setContentsMargins(0, 0, 0, 0);
	buttonsLayout->setSpacing(0);

	if(feature >= 0) {
		widgets::GroupedToolButton *guestButton =
			new widgets::GroupedToolButton(
				widgets::GroupedToolButton::GroupLeft);
		guestButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
		guestButton->setCheckable(true);
		guestButton->setIcon(getTierIcon(int(DP_ACCESS_TIER_GUEST)));
		guestButton->setToolTip(getTierText(int(DP_ACCESS_TIER_GUEST)));
		guestButton->setStatusTip(guestButton->toolTip());
		buttonsLayout->addWidget(guestButton);
		perm.buttons->addButton(guestButton, int(DP_ACCESS_TIER_GUEST));
		perm.combo->addItem(
			guestButton->icon(), guestButton->toolTip(),
			int(DP_ACCESS_TIER_GUEST));

		widgets::GroupedToolButton *authenticatedButton =
			new widgets::GroupedToolButton(
				widgets::GroupedToolButton::GroupCenter);
		authenticatedButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
		authenticatedButton->setCheckable(true);
		authenticatedButton->setIcon(
			getTierIcon(int(DP_ACCESS_TIER_AUTHENTICATED)));
		authenticatedButton->setToolTip(
			getTierText(int(DP_ACCESS_TIER_AUTHENTICATED)));
		authenticatedButton->setStatusTip(authenticatedButton->toolTip());
		buttonsLayout->addWidget(authenticatedButton);
		perm.buttons->addButton(
			authenticatedButton, int(DP_ACCESS_TIER_AUTHENTICATED));
		perm.combo->addItem(
			authenticatedButton->icon(), authenticatedButton->toolTip(),
			int(DP_ACCESS_TIER_AUTHENTICATED));
	} else {
		buttonsLayout->addStretch();
	}

	widgets::GroupedToolButton *trustedButton = new widgets::GroupedToolButton(
		feature >= 0 ? widgets::GroupedToolButton::GroupCenter
					 : widgets::GroupedToolButton::GroupLeft);
	trustedButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	trustedButton->setCheckable(true);
	trustedButton->setIcon(getTierIcon(int(DP_ACCESS_TIER_TRUSTED)));
	trustedButton->setToolTip(getTierText(int(DP_ACCESS_TIER_TRUSTED)));
	trustedButton->setStatusTip(trustedButton->toolTip());
	buttonsLayout->addWidget(trustedButton);
	perm.buttons->addButton(trustedButton, int(DP_ACCESS_TIER_TRUSTED));
	perm.combo->addItem(
		trustedButton->icon(), trustedButton->toolTip(),
		int(DP_ACCESS_TIER_TRUSTED));

	widgets::GroupedToolButton *operatorsButton =
		new widgets::GroupedToolButton(widgets::GroupedToolButton::GroupRight);
	operatorsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
	operatorsButton->setCheckable(true);
	operatorsButton->setIcon(getTierIcon(int(DP_ACCESS_TIER_OPERATOR)));
	operatorsButton->setToolTip(getTierText(int(DP_ACCESS_TIER_OPERATOR)));
	operatorsButton->setStatusTip(operatorsButton->toolTip());
	buttonsLayout->addWidget(operatorsButton);
	perm.buttons->addButton(operatorsButton, int(DP_ACCESS_TIER_OPERATOR));
	perm.combo->addItem(
		operatorsButton->icon(), operatorsButton->toolTip(),
		int(DP_ACCESS_TIER_OPERATOR));

	operatorsButton->setChecked(true);
	perm.combo->setCurrentIndex(perm.combo->count() - 1);

	bool ok;
	int tier = lastPermissions[QString::number(feature)].toInt(&ok);
	if(!ok) {
		tier = feature < 0 ? int(DP_ACCESS_TIER_OPERATOR)
						   : DP_feature_access_tier_default(
								 feature, int(DP_ACCESS_TIER_OPERATOR));
	}
	updateButtonTier(feature, tier);
	updateComboTier(feature, tier);
	connect(
		perm.buttons, &QButtonGroup::idClicked, this,
		std::bind(&Permissions::updateComboTier, this, feature, _1));
	connect(
		perm.combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		[this, feature, combo = perm.combo](int index) {
			updateButtonTier(feature, combo->itemData(index).toInt());
			persist();
		});

	grid->addWidget(perm.icon, row, 0);
	grid->addWidget(perm.label, row, 1);
	grid->addLayout(buttonsLayout, row, 2);
	grid->addWidget(perm.combo, row, 3);
	++row;
}

QLabel *Permissions::makeIconLabel(const QIcon &icon) const
{
	QLabel *label = new QLabel;
	label->setPixmap(
		icon.pixmap(style()->pixelMetric(QStyle::PM_SmallIconSize)));
	return label;
}

void Permissions::updateButtonTier(int feature, int tier)
{
	QHash<int, Perm *>::const_iterator it = m_permsMap.constFind(feature);
	if(it == m_permsMap.constEnd()) {
		qWarning("updateButtonTier: feature %d not found", feature);
		return;
	}

	QButtonGroup *buttons = (*it)->buttons;
	QAbstractButton *button = buttons->button(tier);
	if(button && !button->isChecked()) {
		button->click();
	}
}

void Permissions::updateComboTier(int feature, int tier)
{
	QHash<int, Perm *>::const_iterator it = m_permsMap.constFind(feature);
	if(it == m_permsMap.constEnd()) {
		qWarning("updateComboTier: feature %d not found", feature);
		return;
	}

	QComboBox *combo = (*it)->combo;
	int count = combo->count();
	for(int i = 0; i < count; ++i) {
		bool ok;
		int itemTier = combo->itemData(i).toInt(&ok);
		if(ok && itemTier == tier) {
			if(combo->currentIndex() != i) {
				combo->setCurrentIndex(i);
			}
			break;
		}
	}
}

void Permissions::updateFeatureLimit(int featureLimit, int tier, int value)
{
	QHash<int, Limit *>::const_iterator it =
		m_limitsMap.constFind(featureLimit);
	if(it == m_limitsMap.constEnd()) {
		qWarning(
			"updateFeatureLimit: feature limit %d not found", featureLimit);
		return;
	}

	Limit *limit = it.value();
	for(int i = tier + 1; i < TIER_COUNT; ++i) {
		KisSliderSpinBox *slider = limit->sliders[i];
		if(slider->value() > value) {
			QSignalBlocker blocker(slider);
			slider->setValue(value);
		}
	}

	for(int i = tier - 1; i >= 0; --i) {
		KisSliderSpinBox *slider = limit->sliders[i];
		if(slider->value() < value) {
			QSignalBlocker blocker(slider);
			slider->setValue(value);
		}
	}
}

void Permissions::persist()
{
	QVariantMap permissions;

	for(QHash<int, Perm *>::const_key_value_iterator
			it = m_permsMap.constKeyValueBegin(),
			end = m_permsMap.constKeyValueEnd();
		it != end; ++it) {
		permissions.insert(QString::number(it->first), it->second->tier());
	}

	for(QHash<int, Limit *>::const_key_value_iterator
			it = m_limitsMap.constKeyValueBegin(),
			end = m_limitsMap.constKeyValueEnd();
		it != end; ++it) {
		int featureLimit = it->first;
		const Limit *limit = it->second;
		for(int tier = 0; tier < TIER_COUNT; ++tier) {
			permissions.insert(
				getFeatureLimitKey(featureLimit, tier),
				limit->sliders[tier]->value());
		}
	}

	dpApp().settings().setLastSessionPermissions(permissions);
}

QString Permissions::getAccessTierName(int tier)
{
	return QString::fromUtf8(DP_access_tier_name(tier));
}

QString Permissions::getFeatureName(int feature)
{
	return feature < 0 ? QStringLiteral("deputies")
					   : QString::fromUtf8(DP_feature_name(feature));
}

QString Permissions::getFeatureLimitName(int featureLimit)
{
	return QString::fromUtf8(DP_feature_limit_name(featureLimit));
}

QString Permissions::getFeatureLimitKey(int featureLimit, int tier)
{
	return QString::number((featureLimit * 1000) + (tier * 1000000));
}

int Permissions::getDefaultFeatureTier(int feature)
{
	return feature < 0 ? int(DP_ACCESS_TIER_OPERATOR)
					   : DP_feature_access_tier_default(
							 feature, int(DP_ACCESS_TIER_OPERATOR));
}

int Permissions::getDefaultLimit(int featureLimit, int tier)
{
	int maximum = getLimitMaximum(featureLimit);
	int value = DP_feature_limit_default(featureLimit, tier, maximum);
	return value == -1 ? maximum : value;
}

QIcon Permissions::getTierIcon(int tier)
{
	switch(tier) {
	case DP_ACCESS_TIER_OPERATOR:
		return QIcon::fromTheme("drawpile_security");
	case DP_ACCESS_TIER_TRUSTED:
		return QIcon::fromTheme("checkbox");
	case DP_ACCESS_TIER_AUTHENTICATED:
		return QIcon::fromTheme("im-user");
	default:
		return QIcon::fromTheme("globe");
	}
}

QString Permissions::getTierText(int tier)
{
	switch(tier) {
	case DP_ACCESS_TIER_OPERATOR:
		return tr("Operators");
	case DP_ACCESS_TIER_TRUSTED:
		return tr("Trusted");
	case DP_ACCESS_TIER_AUTHENTICATED:
		return tr("Registered");
	default:
		return tr("Everyone");
	}
}

QString Permissions::getLimitSuffix(int featureLimit)
{
	switch(featureLimit) {
	case DP_FEATURE_LIMIT_BRUSH_SIZE:
		return tr("px");
	default:
		return QString();
	}
}

int Permissions::getLimitMinimum(int featureLimit)
{
	switch(featureLimit) {
	case DP_FEATURE_LIMIT_BRUSH_SIZE:
		return 1;
	case DP_FEATURE_LIMIT_LAYER_COUNT:
		return 0;
	}
	qWarning("getLimitMinimum: unknown feature limit %d", featureLimit);
	return 0;
}

int Permissions::getLimitMaximum(int featureLimit)
{
	switch(featureLimit) {
	case DP_FEATURE_LIMIT_BRUSH_SIZE:
		return 1000;
	case DP_FEATURE_LIMIT_LAYER_COUNT:
		return 32767;
	}
	qWarning("getLimitMaximum: unknown feature limit %d", featureLimit);
	return 0;
}

qreal Permissions::getLimitExponentRatio(int featureLimit)
{
	switch(featureLimit) {
	case DP_FEATURE_LIMIT_BRUSH_SIZE:
	case DP_FEATURE_LIMIT_LAYER_COUNT:
		return 3.0;
	}
	qWarning("getLimitExponentRatio: unknown feature limit %d", featureLimit);
	return 1.0;
}

}
}
}
