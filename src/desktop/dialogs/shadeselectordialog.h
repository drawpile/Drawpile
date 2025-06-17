// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SHADESELECTORDIALOG_H
#define DESKTOP_DIALOGS_SHADESELECTORDIALOG_H
#include <QDialog>
#include <QFrame>
#include <QHash>
#include <QVariantHash>
#include <QVariantList>

class KisSliderSpinBox;
class QCheckBox;
class QComboBox;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;

namespace widgets {
class GroupedToolButton;
class ShadeSelector;
}

namespace dialogs {

class ShadeSelectorDialog final : public QDialog {
	Q_OBJECT
public:
	explicit ShadeSelectorDialog(
		const QColor &color, QWidget *parent = nullptr);

private:
	class RowWidget final : public QFrame {
	public:
		RowWidget(ShadeSelectorDialog *dlg, int index);

		void setFromComfig(const QVariantHash &rowConfig);
		QVariantHash toConfig() const;

		void setMovable(bool up, bool down);

		void swapWith(RowWidget *rw);

	private:
		void updateSliders(int index);
		void purgeSliders();
		void initSliders();

		void addSlider(const QString &prefix, int defaultValue);

		void stashSliderValue(int preset, int sliderIndex);

		int
		unstashSliderValue(int preset, int sliderIndex, int defaultValue) const;

		static int getStashKey(int preset, int sliderIndex);
		static QStringList getCustomConfigKeys();

		ShadeSelectorDialog *m_dlg;
		QComboBox *m_combo;
		QVector<KisSliderSpinBox *> m_sliders;
		widgets::GroupedToolButton *m_moveUpButton;
		widgets::GroupedToolButton *m_moveDownButton;
		QHash<int, int> m_stashedSliderValues;
		int m_preset = -1;
	};
	friend class RowWidget;

	void updateConfig();

	QVariantList getConfig() const;

	void swapRows(int i, int j);

	QString
	getColorSpaceText2(const QString &hsvHslText, const QString &lchText) const;

	QString getColorSpaceText3(
		const QString &hsvText, const QString &hslText,
		const QString &lchText) const;

	void setColorShadesEnabled(bool enabled);
	void setRowCount(int rowCount);

	void pickColor();

	void saveToSettings();

	QCheckBox *m_colorShadesEnabledBox;
	QStackedWidget *m_stack;
	KisSliderSpinBox *m_rowCountSpinner;
	KisSliderSpinBox *m_rowHeightSpinner;
	KisSliderSpinBox *m_columnCountSpinner;
	KisSliderSpinBox *m_borderThicknessSpinner;
	widgets::ShadeSelector *m_shadeSelector;
	QScrollArea *m_scroll;
	QVBoxLayout *m_rowsLayout;
	QVariantList m_rowsConfig;
	QVector<RowWidget *> m_rowWidgets;
	QVector<RowWidget *> m_spareRowWidgets;
	int m_colorSpace;
};

}


#endif
