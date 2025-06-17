// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_STARTDIALOG_HOST_PERMISSIONS_H
#define DESKTOP_DIALOGS_STARTDIALOG_HOST_PERMISSIONS_H
#include <QHash>
#include <QJsonObject>
#include <QPair>
#include <QWidget>

class KisSliderSpinBox;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QScrollArea;

namespace dialogs {
namespace startdialog {
namespace host {

class Permissions : public QWidget {
	Q_OBJECT
public:
	explicit Permissions(QWidget *parent = nullptr);

	void reset();
	void load(const QJsonObject &json);
	QJsonObject save() const;

	void host(
		int &outUndoLimit, QHash<int, int> &outFeaturePermissions,
		bool &outDeputies, QHash<int, QHash<int, int>> &outFeatureLimits);

private:
	static constexpr int TIER_COUNT = 4;

	struct Limit {
		QLabel *icon;
		QLabel *label;
		KisSliderSpinBox *sliders[TIER_COUNT];
	};

	struct Perm {
		QLabel *icon;
		QLabel *label;
		QButtonGroup *buttons;
		QComboBox *combo;

		int tier() const;
		void setTier(int tier);
	};

	void addLimit(
		QGridLayout *grid, int &row, const QVariantMap &lastPermissions,
		Limit &limit, int featureLimit, const QIcon &icon, const QString &text);

	void addPerm(
		QGridLayout *grid, int &row, const QVariantMap &lastPermissions,
		Perm &perm, int feature, const QIcon &icon, const QString &text);

	QLabel *makeIconLabel(const QIcon &icon) const;

	void updateButtonTier(int feature, int tier);
	void updateComboTier(int feature, int tier);
	void updateFeatureLimit(int featureLimit, int tier, int value);
	void persist();

	static QString getAccessTierName(int tier);
	static QString getFeatureName(int feature);
	static QString getFeatureLimitName(int featureLimit);
	static QString getFeatureLimitKey(int featureLimit, int tier);
	static int getDefaultFeatureTier(int feature);
	static int getDefaultLimit(int featureLimit, int tier);
	static QIcon getTierIcon(int tier);
	static QString getTierText(int tier);
	static QString getLimitSuffix(int featureLimit);
	static int getLimitMinimum(int featureLimit);
	static int getLimitMaximum(int featureLimit);
	static qreal getLimitExponentRatio(int featureLimit);

	KisSliderSpinBox *m_undoLimitSpinner;
	Perm m_permPutImage;
	Perm m_permRegionMove;
	Perm m_permResize;
	Perm m_permBackground;
	Perm m_permEditLayers;
	Perm m_permOwnLayers;
	Perm m_permCreateAnnotation;
	Perm m_permLaser;
	Perm m_permUndo;
	Perm m_permPigment;
	Perm m_permTimeline;
	Perm m_permMypaint;
	Perm m_permKickBan;
	Limit m_limitBrushSize;
	Limit m_limitLayerCount;
	QHash<int, Perm *> m_permsMap;
	QHash<int, Limit *> m_limitsMap;
};

}
}
}

#endif
