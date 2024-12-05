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
		bool &outDeputies);

private:
	struct Perm {
		QLabel *icon;
		QLabel *label;
		QButtonGroup *buttons;
		QComboBox *combo;

		int tier() const;
		void setTier(int tier);
	};

	void addPerm(
		QGridLayout *grid, int &row, const QVariantMap &lastPermissions,
		Perm &perm, int feature, const QIcon &icon, const QString &text);

	void updateButtonTier(int feature, int tier);
	void updateComboTier(int feature, int tier);
	void persistTiers();

	static QString getFeatureName(int feature);
	static int getDefaultFeatureTier(int feature);

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
	Perm m_permTimeline;
	Perm m_permMypaint;
	Perm m_permKickBan;
	QHash<int, Perm *> m_permsByFeature;
	bool m_updatingTier = false;
};

}
}
}

#endif
