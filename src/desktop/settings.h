// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SETTINGS_H
#define DESKTOP_SETTINGS_H

#include "desktop/dialogs/invitedialog.h"
#include "desktop/scene/canvasview.h"
#include "desktop/tabletinput.h"
#include "desktop/view/cursor.h"
#include "libclient/export/videoexporter.h"
#include "libclient/settings.h"
#include "libclient/tools/tool.h"

#include <QAbstractButton>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QAction>
#include <QButtonGroup>
#include <QComboBox>
#include <QColor>
#include <QLineEdit>
#include <QObject>
#include <QPlainTextEdit>
#include <QSize>
#include <QVariant>
#include <QtColorWidgets/ColorWheel>
#include <optional>
#include <utility>

namespace desktop {
Q_NAMESPACE
namespace settings {
Q_NAMESPACE

enum class InterfaceMode : int {
	Unknown,
	Desktop,
	SmallScreen,
};
Q_ENUM_NS(InterfaceMode)

enum class KineticScrollGesture : int {
	None,
	LeftClick,
	MiddleClick,
	RightClick,
	Touch,
};
Q_ENUM_NS(KineticScrollGesture)

enum class ThemePalette : int {
	System,
	Light,
	Dark,
	KritaBright,
	KritaDark,
	KritaDarker,
	Fusion,
	HotdogStand
};
Q_ENUM_NS(ThemePalette)

class Settings final : public libclient::settings::Settings {
	Q_OBJECT

public:
	Settings(QObject *parent = nullptr);

#	define DP_SETTINGS_HEADER
#	include "desktop/settings_table.h"
#	undef DP_SETTINGS_HEADER

// This is to make sure that settings inherited from libclient can use the
// widget bindings. The known alternative is CRTP which just causes lots of
// problems. Maybe there is a better option like making some free functions
// instead, but this works for now.
#	define DP_SETTINGS_REBIND
#	include "libclient/settings_table.h"
#	undef DP_SETTINGS_REBIND

protected:
	template <typename T, typename Widget, typename Self>
	inline std::enable_if_t<
		std::is_base_of_v<QAction, Widget> || std::is_base_of_v<QAbstractButton, Widget>,
		std::pair<QMetaObject::Connection, QMetaObject::Connection>
	> bind(T initialValue, void (Self::*changed)(T), void (Self::*setter)(T), Widget *widget) {
		return bindAs<bool>(initialValue, changed, setter, widget, &Widget::setChecked, &Widget::toggled);
	}

	template <typename T, typename Self>
	inline auto bind(T initialValue, void (Self::*changed)(T), void (Self::*setter)(T), QAbstractSlider *widget) {
		return bindAs<int>(initialValue, changed, setter, widget, &QAbstractSlider::setValue, &QAbstractSlider::valueChanged);
	}

	template <typename T, typename Self>
	inline auto bind(T initialValue, void (Self::*changed)(T), void (Self::*setter)(T), QButtonGroup *widget) {
		static_assert(std::is_base_of_v<::libclient::settings::Settings, Self>);

		const auto slot = [=](T value) {
			if (auto *button = widget->button(int(value))) {
				button->setChecked(true);
			} else {
				qWarning() << "bound invalid value" << value << "to QButtonGroup";
			}
		};

		std::invoke(slot, initialValue);

		return std::pair {
			connect(this, changed, widget, std::move(slot)),
			// TODO: Qt 5.15+ minimum version would allow using `idToggled`
			// instead
			connect(
				widget, QOverload<QAbstractButton *, bool>::of(&QButtonGroup::buttonToggled),
				this, [=](QAbstractButton *button, bool checked) {
					if (checked) {
						std::invoke(setter, this, T(widget->id(button)));
					}
				}
			)
		};
	}

	template <typename T, typename Self>
	inline auto bind(T initialValue, void (Self::*changed)(T), void (Self::*setter)(T), QComboBox *widget, std::nullopt_t) {
		return bindAs<int>(initialValue, changed, setter, widget, &QComboBox::setCurrentIndex, QOverload<int>::of(&QComboBox::currentIndexChanged));
	}

	template <typename T, typename Self>
	inline auto bind(T initialValue, void (Self::*changed)(T), void (Self::*setter)(T), QComboBox *widget, int role) {
		const auto slot = [=](T value) {
			const auto variant = QVariant::fromValue(value);
			const auto index = widget->findData(variant, role);
			if (index == -1) {
				qWarning() << "bound invalid value" << variant << "to QComboBox";
			} else {
				widget->setCurrentIndex(index);
			}
		};

		std::invoke(slot, initialValue);

		return std::pair {
			connect(this, changed, widget, std::move(slot)),
			connect(widget, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int index) {
				std::invoke(setter, this, widget->itemData(index, role).template value<T>());
			})
		};
	}

	template <typename T, typename Self>
	inline std::enable_if_t<std::is_integral_v<T> || std::is_floating_point_v<T>,
		std::pair<QMetaObject::Connection, QMetaObject::Connection>
	> bind(T initialValue, void (Self::*changed)(T), void (Self::*setter)(T), QLineEdit *widget) {
		const auto slot = [=](T value) {
			widget->setText(QString::number(value));
		};
		std::invoke(slot, initialValue);
		return std::pair {
			connect(this, changed, widget, slot),
			connect(widget, &QLineEdit::editingFinished, this, [=] {
				std::invoke(setter, this, widget->text().toInt());
			})
		};
	}

	template <typename T, typename Self>
	inline std::enable_if_t<
		std::is_convertible_v<T, QString>,
		std::pair<QMetaObject::Connection, QMetaObject::Connection>
	> bind(T initialValue, void (Self::*changed)(T), void (Self::*setter)(T), QLineEdit *widget) {
		widget->setText(initialValue);
		return std::pair {
			connect(this, changed, widget, &QLineEdit::setText),
			connect(widget, &QLineEdit::editingFinished, this, [=] {
				std::invoke(setter, this, widget->text());
			})
		};
	}

	template <typename T, typename Self>
	inline std::enable_if_t<
		std::is_convertible_v<T, QString>,
		std::pair<QMetaObject::Connection, QMetaObject::Connection>
	> bind(T initialValue, void (Self::*changed)(T), void (Self::*setter)(T), QPlainTextEdit *widget) {
		widget->setPlainText(initialValue);
		return std::pair {
			connect(this, changed, widget, [=](const QString &text) {
				// Setting the text moves the caret back to the beginning and
				// purges the edit history, so we only set if necessary.
				if(widget->toPlainText() != text) {
					widget->setPlainText(text);
				}
			}),
			connect(widget, &QPlainTextEdit::textChanged, this, [=] {
				std::invoke(setter, this, widget->toPlainText());
			})
		};
	}

	template <typename T, typename SpinBox, typename Self>
	inline std::enable_if_t<std::is_base_of_v<QAbstractSpinBox, SpinBox>,
		std::pair<QMetaObject::Connection, QMetaObject::Connection>
	> bind(T initialValue, void (Self::*changed)(T), void (Self::*setter)(T), SpinBox *widget) {
		widget->setValue(initialValue);
		return std::pair {
			connect(this, changed, widget, &SpinBox::setValue),
			connect(widget, QOverload<T>::of(&SpinBox::valueChanged), this, setter)
		};
	}

	using libclient::settings::Settings::bind;
};

void initializeTypes();

} // namespace settings
} // namespace desktop

#endif
