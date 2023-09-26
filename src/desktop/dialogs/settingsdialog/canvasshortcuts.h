// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_DIALOGS_SETTINGSDIALOG_CANVASSHORTCUTS_H
#define DESKTOP_DIALOGS_SETTINGSDIALOG_CANVASSHORTCUTS_H
#include <QItemEditorFactory>
#include <QWidget>

class QLineEdit;
class QVBoxLayout;

namespace desktop {
namespace settings {
class Settings;
}
}

namespace dialogs {
namespace settingsdialog {

class CanvasShortcuts final : public QWidget {
	Q_OBJECT
public:
	CanvasShortcuts(
		desktop::settings::Settings &settings, QWidget *parent = nullptr);

private:
	void initCanvasShortcuts(
		desktop::settings::Settings &settings, QVBoxLayout *form,
		QLineEdit *filter);
};

} // namespace settingsdialog
} // namespace dialogs

#endif
