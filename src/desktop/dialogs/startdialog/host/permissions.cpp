// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/startdialog/host/permissions.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/widgets/groupedtoolbutton.h"
#include "desktop/widgets/kis_slider_spin_box.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QStylePainter>
#include <QVBoxLayout>
#include <functional>

using std::placeholders::_1;

namespace dialogs {
namespace startdialog {
namespace host {

namespace {

// Ignores wheel events so that scrolling over combo boxes doesn't end up
// changing their contents, but instead scrolls the scroll area around them.
class PermissionWidget : public QWidget {
public:
	explicit PermissionWidget(QWidget *parent = nullptr)
		: QWidget(parent)
	{
	}

protected:
	void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};

// Ignores wheel events in collusion with the above. Also doesn't show icons on
// the combo box itself, since those are already on the buttons next to it, so
// having them in the combo box adds too much visual noise.
class PermissionComboBox : public QComboBox {
public:
	explicit PermissionComboBox(QWidget *parent = nullptr)
		: QComboBox(parent)
	{
	}

protected:
	void wheelEvent(QWheelEvent *event) override { event->ignore(); }

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

}

Permissions::Permissions(QWidget *parent)
	: QWidget(parent)
{
	setContentsMargins(0, 0, 0, 0);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	m_undoLimitSpinner = new KisSliderSpinBox;
	m_undoLimitSpinner->setPrefix(tr("Undo Limit: "));
	m_undoLimitSpinner->setRange(
		DP_CANVAS_HISTORY_UNDO_DEPTH_MIN, DP_CANVAS_HISTORY_UNDO_DEPTH_MAX);
	layout->addWidget(m_undoLimitSpinner);

	QScrollArea *permissionScroll = new QScrollArea;
	permissionScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	permissionScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	utils::bindKineticScrolling(permissionScroll);
	layout->addWidget(permissionScroll, 1);

	QWidget *permissionsWidget = new PermissionWidget;
	permissionScroll->setWidgetResizable(true);
	permissionScroll->setWidget(permissionsWidget);

	QVBoxLayout *permissionsLayout = new QVBoxLayout(permissionsWidget);
	QGridLayout *grid = new QGridLayout;
	grid->setContentsMargins(0, 0, 0, 0);
	grid->setColumnStretch(3, 1);
	permissionsLayout->addLayout(grid);

	int row = 0;
	desktop::settings::Settings &settings = dpApp().settings();
	QVariantMap lastPermissions = settings.lastSessionPermissions();
	addPerm(
		grid, row, lastPermissions, m_permPutImage, int(DP_FEATURE_PUT_IMAGE),
		QIcon::fromTheme("fill-color"), tr("Cut, paste and fill:"));
	addPerm(
		grid, row, lastPermissions, m_permRegionMove,
		int(DP_FEATURE_REGION_MOVE), QIcon::fromTheme("drawpile_transform"),
		tr("Transform selections:"));
	addPerm(
		grid, row, lastPermissions, m_permCreateAnnotation,
		int(DP_FEATURE_CREATE_ANNOTATION), QIcon::fromTheme("draw-text"),
		tr("Create annotations:"));
	addPerm(
		grid, row, lastPermissions, m_permOwnLayers, int(DP_FEATURE_OWN_LAYERS),
		QIcon::fromTheme("layer-visible-on"), tr("Manage own layers:"));
	addPerm(
		grid, row, lastPermissions, m_permEditLayers,
		int(DP_FEATURE_EDIT_LAYERS), QIcon::fromTheme("configure"),
		tr("Manage all layers:"));
	addPerm(
		grid, row, lastPermissions, m_permResize, int(DP_FEATURE_RESIZE),
		QIcon::fromTheme("zoom-select"), tr("Change canvas size:"));
	addPerm(
		grid, row, lastPermissions, m_permBackground,
		int(DP_FEATURE_BACKGROUND), QIcon::fromTheme("edit-image"),
		tr("Change background:"));
	addPerm(
		grid, row, lastPermissions, m_permMypaint, int(DP_FEATURE_MYPAINT),
		QIcon::fromTheme("drawpile_mypaint"), tr("MyPaint brushes:"));
	addPerm(
		grid, row, lastPermissions, m_permLaser, int(DP_FEATURE_LASER),
		QIcon::fromTheme("cursor-arrow"), tr("Laser pointer:"));
	addPerm(
		grid, row, lastPermissions, m_permTimeline, int(DP_FEATURE_TIMELINE),
		QIcon::fromTheme("keyframe"), tr("Animation timeline:"));
	addPerm(
		grid, row, lastPermissions, m_permUndo, int(DP_FEATURE_UNDO),
		QIcon::fromTheme("edit-undo"), tr("Undo and redo:"));
	addPerm(
		grid, row, lastPermissions, m_permKickBan, -1,
		QIcon::fromTheme("drawpile_ban"), tr("Kick and ban:"));

	permissionsLayout->addStretch();

	settings.bindLastSessionUndoDepth(m_undoLimitSpinner);
}

void Permissions::reset()
{
	desktop::settings::Settings &settings = dpApp().settings();
	settings.setLastSessionUndoDepth(SESSION_UNDO_LIMIT_DEFAULT);
	settings.setLastSessionPermissions(QVariantMap());
	for(QHash<int, Perm *>::key_value_iterator
			it = m_permsByFeature.keyValueBegin(),
			end = m_permsByFeature.keyValueEnd();
		it != end; ++it) {
		int feature = it->first;
		Perm *perm = it->second;
		perm->setTier(getDefaultFeatureTier(feature));
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
	for(QHash<int, Perm *>::key_value_iterator
			it = m_permsByFeature.keyValueBegin(),
			end = m_permsByFeature.keyValueEnd();
		it != end; ++it) {
		int feature = it->first;
		Perm *perm = it->second;
		int tier = json[getFeatureName(feature)].toInt(-1);
		if(tier >= 0) {
			perm->setTier(tier);
		}
	}
}

QJsonObject Permissions::save() const
{
	QJsonObject json;
	json[QStringLiteral("undolimit")] = m_undoLimitSpinner->value();
	for(QHash<int, Perm *>::const_key_value_iterator
			it = m_permsByFeature.constKeyValueBegin(),
			end = m_permsByFeature.constKeyValueEnd();
		it != end; ++it) {
		int feature = it->first;
		const Perm *perm = it->second;
		json[getFeatureName(feature)] = perm->tier();
	}
	return json;
}

void Permissions::host(
	int &outUndoLimit, QHash<int, int> &outFeaturePermissions,
	bool &outDeputies)
{
	outUndoLimit = m_undoLimitSpinner->value();
	for(QHash<int, Perm *>::const_key_value_iterator
			it = m_permsByFeature.constKeyValueBegin(),
			end = m_permsByFeature.constKeyValueEnd();
		it != end; ++it) {
		int feature = it->first;
		if(feature >= 0 && feature < DP_FEATURE_COUNT) {
			outFeaturePermissions.insert(feature, it->second->tier());
		}
	}
	outDeputies = m_permKickBan.tier() != DP_ACCESS_TIER_OPERATOR;
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

void Permissions::addPerm(
	QGridLayout *grid, int &row, const QVariantMap &lastPermissions, Perm &perm,
	int feature, const QIcon &icon, const QString &text)
{
	m_permsByFeature.insert(feature, &perm);
	perm.icon = new QLabel;
	perm.label = new QLabel(text);
	perm.buttons = new QButtonGroup(this);
	perm.combo = new PermissionComboBox;

	perm.icon->setPixmap(
		icon.pixmap(style()->pixelMetric(QStyle::PM_SmallIconSize)));

	QHBoxLayout *buttonsLayout = new QHBoxLayout;
	buttonsLayout->setContentsMargins(0, 0, 0, 0);
	buttonsLayout->setSpacing(0);

	if(feature >= 0) {
		widgets::GroupedToolButton *guestButton =
			new widgets::GroupedToolButton(
				widgets::GroupedToolButton::GroupLeft);
		guestButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
		guestButton->setCheckable(true);
		guestButton->setIcon(QIcon::fromTheme("globe"));
		guestButton->setToolTip(tr("Everyone"));
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
		authenticatedButton->setIcon(QIcon::fromTheme("im-user"));
		authenticatedButton->setToolTip(tr("Registered"));
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
	trustedButton->setIcon(QIcon::fromTheme("checkbox"));
	trustedButton->setToolTip(tr("Trusted"));
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
	operatorsButton->setIcon(QIcon::fromTheme("drawpile_security"));
	operatorsButton->setToolTip(tr("Operators"));
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
			persistTiers();
		});

	grid->addWidget(perm.icon, row, 0);
	grid->addWidget(perm.label, row, 1);
	grid->addLayout(buttonsLayout, row, 2);
	grid->addWidget(perm.combo, row, 3);
	++row;
}

void Permissions::updateButtonTier(int feature, int tier)
{
	QHash<int, Perm *>::const_iterator it = m_permsByFeature.constFind(feature);
	if(it == m_permsByFeature.constEnd()) {
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
	QHash<int, Perm *>::const_iterator it = m_permsByFeature.constFind(feature);
	if(it == m_permsByFeature.constEnd()) {
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

void Permissions::persistTiers()
{
	QVariantMap permissions;
	for(QHash<int, Perm *>::const_key_value_iterator
			it = m_permsByFeature.constKeyValueBegin(),
			end = m_permsByFeature.constKeyValueEnd();
		it != end; ++it) {
		permissions.insert(QString::number(it->first), it->second->tier());
	}
	dpApp().settings().setLastSessionPermissions(permissions);
}

QString Permissions::getFeatureName(int feature)
{
	return feature < 0 ? QStringLiteral("deputies")
					   : QString::fromUtf8(DP_feature_name(feature));
}

int Permissions::getDefaultFeatureTier(int feature)
{
	return feature < 0 ? int(DP_ACCESS_TIER_OPERATOR)
					   : DP_feature_access_tier_default(
							 feature, int(DP_ACCESS_TIER_OPERATOR));
}

}
}
}
