// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_TOUCH_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_TOUCH_H
#include "desktop/dialogs/settingsdialog/page.h"
#include <functional>

class QComboBox;
class QDialogButtonBox;
class QFormLayout;
class QLineEdit;
class QPushButton;

namespace dialogs {
namespace settingsdialog {

class Touch final : public Page {
	Q_OBJECT
public:
	Touch(config::Config *cfg, QWidget *parent = nullptr);

	void createButtons(QDialogButtonBox *buttons);
	void showButtons();

	static void
	addTouchPressureSettingTo(config::Config *cfg, QFormLayout *form);

signals:
	void touchTesterRequested();

protected:
	void setUp(config::Config *cfg, QVBoxLayout *layout) override;

private:
	class TapActionWidget : public QWidget {
	public:
		using GetTriggerFn = std::function<QString(config::Config *)>;
		using SetTriggerFn =
			std::function<void(config::Config *, const QString &)>;

		explicit TapActionWidget(
			config::Config *cfg, const GetTriggerFn &getTrigger,
			const SetTriggerFn &setTrigger, QWidget *parent = nullptr);

		void updateTapAction(int tapAction);
		void updateTriggerLabel(const QString &trigger);

	private:
		void changeTrigger();
		void setTriggerInConfig(const QString &trigger);

		config::Config *m_cfg;
		GetTriggerFn m_getTrigger;
		SetTriggerFn m_setTrigger;
		QLineEdit *m_lineEdit;
		QPushButton *m_changeButton;
	};

	void initMode(config::Config *cfg, QFormLayout *form);

	void initTapActions(config::Config *cfg, QFormLayout *form);

	void initTapExtraActions(config::Config *cfg, QFormLayout *form);

	void initTouchActions(config::Config *cfg, QFormLayout *form);

	static QComboBox *makeTapCombo();

	QPushButton *m_touchTesterButton;
};

}
}

#endif
