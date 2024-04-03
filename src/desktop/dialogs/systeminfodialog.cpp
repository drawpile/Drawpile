// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/dialogs/systeminfodialog.h"
#include "cmake-config/config.h"
#include "desktop/main.h"
#include "desktop/tabletinput.h"
#include "desktop/utils/widgetutils.h"
#include "desktop/view/glcanvas.h"
#include <QClipboard>
#include <QDialogButtonBox>
#include <QFont>
#include <QLibraryInfo>
#include <QMetaEnum>
#include <QPushButton>
#include <QScreen>
#include <QStringList>
#include <QSurfaceFormat>
#include <QSysInfo>
#include <QTextEdit>
#include <QVBoxLayout>

namespace dialogs {

SystemInfoDialog::SystemInfoDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(tr("System Information"));
	QVBoxLayout *layout = new QVBoxLayout(this);

	m_textEdit = new QTextEdit;
	QFont font = m_textEdit->font();
	font.setFamily(QStringLiteral("monospace"));
	font.setStyleHint(QFont::TypeWriter);
	m_textEdit->setFont(font);
	m_textEdit->setReadOnly(true);
	m_textEdit->setPlainText(getSystemInfo());
	utils::initKineticScrolling(m_textEdit);
	layout->addWidget(m_textEdit, 1);

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
	QPushButton *copyButton =
		buttons->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
	connect(
		copyButton, &QPushButton::clicked, this,
		&SystemInfoDialog::copyToClipboard);
	connect(
		buttons, &QDialogButtonBox::accepted, this, &SystemInfoDialog::accept);
	connect(
		buttons, &QDialogButtonBox::rejected, this, &SystemInfoDialog::reject);
	layout->addWidget(buttons);

	resize(600, 400);
}

QString SystemInfoDialog::getSystemInfo() const
{
	QString info;

	info += QStringLiteral("SYSTEM INFORMATION\n");
	info += QStringLiteral("\n");

	info += QStringLiteral("Drawpile %1\n").arg(cmake_config::version());
	info += QStringLiteral("Qt: %1 (%2)\n")
				.arg(
					QLibraryInfo::version().toString(),
					QLibraryInfo::isDebugBuild() ? QStringLiteral("debug")
												 : QStringLiteral("release"));
	info += QStringLiteral("Features: %1\n").arg(getCompileFeatures());
	info += QStringLiteral("\n");

	info += QStringLiteral("OS: %1\n").arg(QSysInfo::prettyProductName());
	info += QStringLiteral("ABI: %1\n").arg(QSysInfo::buildAbi());
	info +=
		QStringLiteral("Arch: %1\n").arg(QSysInfo::currentCpuArchitecture());
	info += QStringLiteral("Build arch: %1\n")
				.arg(QSysInfo::buildCpuArchitecture());
	info += QStringLiteral("Endianness: ");
	switch(QSysInfo::ByteOrder) {
	case QSysInfo::LittleEndian:
		info += QStringLiteral("little");
		break;
	case QSysInfo::BigEndian:
		info += QStringLiteral("big");
		break;
	default:
		info += QStringLiteral("unknown");
		break;
	}
	info += QStringLiteral("\n");
	info += QStringLiteral("Word size: %1\n").arg(int(QSysInfo::WordSize));
	info += QStringLiteral("\n");

	info += QStringLiteral("Interface mode: %1\n")
				.arg(
					dpApp().smallScreenMode() ? QStringLiteral("small-screen")
											  : QStringLiteral("desktop"));
	info += QStringLiteral("Device pixel ratio: %1\n").arg(devicePixelRatioF());
	desktop::settings::Settings &settings = dpApp().settings();
	QSettings *scalingSettings = settings.scalingSettings();
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QString highDpiScaling = QStringLiteral("%1").arg(boolToString(
		scalingSettings->value(QStringLiteral("enabled")).toBool()));
#else
	QString highDpiScaling = QStringLiteral("always enabled");
#endif
	info += QStringLiteral("High-DPI scaling: %1\n").arg(highDpiScaling);
	info += QStringLiteral("Override scaling setting: %1\n")
				.arg(
					scalingSettings->value(QStringLiteral("override")).toBool()
						? QStringLiteral("%1%").arg(
							  scalingSettings->value(QStringLiteral("factor"))
								  .toInt())
						: QStringLiteral("disabled"));
	info += QStringLiteral("Override font size: %1\n")
				.arg(
					settings.overrideFontSize()
						? QStringLiteral("%1pt").arg(settings.fontSize())
						: QStringLiteral("disabled"));
	info += QStringLiteral("Vertical sync setting: %1\n")
				.arg(scalingSettings->value(QStringLiteral("vsync")).toInt());
	info += QStringLiteral("QT_OPENGL: \"%1\"\n")
				.arg(QString::fromUtf8(qgetenv("QT_OPENGL")));
	info += QStringLiteral("QT_SCALE_FACTOR: \"%1\"\n")
				.arg(QString::fromUtf8(qgetenv("QT_SCALE_FACTOR")));
	info += QStringLiteral("DRAWPILE_VSYNC: \"%1\"\n")
				.arg(QString::fromUtf8(qgetenv("DRAWPILE_VSYNC")));
	info += QStringLiteral("\n");

	QSurfaceFormat format = QSurfaceFormat::defaultFormat();
	info += QStringLiteral("Default format type %1 %2.%3 %4\n")
				.arg(QMetaEnum::fromType<QSurfaceFormat::RenderableType>()
						 .valueToKey(format.renderableType()))
				.arg(format.majorVersion())
				.arg(format.minorVersion())
				.arg(QMetaEnum::fromType<QSurfaceFormat::OpenGLContextProfile>()
						 .valueToKey(format.profile()));
	info += QStringLiteral("Default format buffer sizes: A=%1 R=%2 G=%3 B=%4 "
						   "depth=%5 stencil=%6\n")
				.arg(format.alphaBufferSize())
				.arg(format.redBufferSize())
				.arg(format.greenBufferSize())
				.arg(format.greenBufferSize())
				.arg(format.depthBufferSize())
				.arg(format.stencilBufferSize());
	info += QStringLiteral("Default format swap interval: %1\n")
				.arg(format.swapInterval());
	info +=
		QStringLiteral("Default format swap behavior: %1\n")
			.arg(QMetaEnum::fromType<QSurfaceFormat::SwapBehavior>().valueToKey(
				format.swapBehavior()));
	info +=
		QStringLiteral("Canvas renderer: %1\n")
			.arg(
				QMetaEnum::fromType<libclient::settings::CanvasImplementation>()
					.valueToKey(dpApp().canvasImplementation()));
	info += QStringLiteral("Render smoothing: %1\n")
				.arg(boolToString(settings.renderSmooth()));
	info += QStringLiteral("Pixel jitter prevention: %1\n")
				.arg(boolToString(settings.renderUpdateFull()));
	for(const QString &s : view::GlCanvas::getSystemInfo()) {
		info += QStringLiteral("%1\n").arg(s);
	}
	info += QStringLiteral("\n");

	info += QStringLiteral("Tablet events: %1\n")
				.arg(boolToString(settings.tabletEvents()));
	info += QStringLiteral("Tablet driver: %1\n").arg(tabletinput::current());
	info +=
		QStringLiteral("Eraser action: %1\n")
			.arg(QMetaEnum::fromType<tabletinput::EraserAction>().valueToKey(
				settings.tabletEraserAction()));
	info += QStringLiteral("Ignore zero-pressure inputs: %1\n")
				.arg(boolToString(settings.ignoreZeroPressureInputs()));
	info += QStringLiteral("Compensate jagged curves: %1\n")
				.arg(boolToString(settings.interpolateInputs()));
	info += QStringLiteral("Global smoothing: %1\n").arg(settings.smoothing());
	info += QStringLiteral("Global pressure curve: \"%1\"\n")
				.arg(settings.globalPressureCurve());
	info += QStringLiteral("One-finger draw: %1\n")
				.arg(boolToString(settings.oneFingerDraw()));
	info += QStringLiteral("One-finger scroll: %1\n")
				.arg(boolToString(settings.oneFingerScroll()));
	info += QStringLiteral("Two-finger zoom: %1\n")
				.arg(boolToString(settings.twoFingerZoom()));
	info += QStringLiteral("Two-finger rotate: %1\n")
				.arg(boolToString(settings.twoFingerRotate()));
	info += QStringLiteral("Touch gestures: %1\n")
				.arg(boolToString(settings.touchGestures()));
	info += QStringLiteral("\n");

	int screenNumber = 1;
	for(const QScreen *screen : QGuiApplication::screens()) {
		info += QStringLiteral("Screen %1").arg(screenNumber);
		if(screen == QGuiApplication::primaryScreen()) {
			info += QStringLiteral(" (primary)");
		}
		info += QStringLiteral("\n");
		info += QStringLiteral("Physical size: %1x%2mm\n")
					.arg(screen->physicalSize().width())
					.arg(screen->physicalSize().height());
		info += QStringLiteral("Geometry: (%1, %2) %3x%4px\n")
					.arg(screen->geometry().x())
					.arg(screen->geometry().y())
					.arg(screen->geometry().width())
					.arg(screen->geometry().height());
		info += QStringLiteral("Available geometry: (%1, %2) %3x%4px\n")
					.arg(screen->availableGeometry().x())
					.arg(screen->availableGeometry().y())
					.arg(screen->availableGeometry().width())
					.arg(screen->availableGeometry().height());
		info += QStringLiteral("Virtual geometry: (%1, %2) %3x%4px\n")
					.arg(screen->virtualGeometry().x())
					.arg(screen->virtualGeometry().y())
					.arg(screen->virtualGeometry().width())
					.arg(screen->virtualGeometry().height());
		info += QStringLiteral("Available virtual geometry: (%1, %2) %3x%4px\n")
					.arg(screen->availableVirtualGeometry().x())
					.arg(screen->availableVirtualGeometry().y())
					.arg(screen->availableVirtualGeometry().width())
					.arg(screen->availableVirtualGeometry().height());
		info += QStringLiteral("Dots per inch: %1\n")
					.arg(screen->physicalDotsPerInch());
		info += QStringLiteral("Device pixel ratio: %1\n")
					.arg(screen->devicePixelRatio());
		info +=
			QStringLiteral("Refresh rate: %1Hz\n").arg(screen->refreshRate());
		info += QStringLiteral("\n");
		++screenNumber;
	}

	info += "END OF SYSTEM INFORMATION";
	return info;
}

QString SystemInfoDialog::getCompileFeatures()
{
	QStringList features;
#ifdef DRAWPILE_SOURCE_ASSETS_DESKTOP
	features.append(QStringLiteral("SOURCE_ASSETS"));
#endif
#ifdef HAVE_TCPSOCKETS
	features.append(QStringLiteral("HAVE_TCPSOCKETS"));
#endif
#ifdef HAVE_WEBSOCKETS
	features.append(QStringLiteral("HAVE_WEBSOCKETS"));
#endif
#ifdef DP_HAVE_BUILTIN_SERVER
	features.append(QStringLiteral("HAVE_BUILTIN_SERVER"));
#endif
#ifdef HAVE_CHAT_LINE_EDIT_MOBILE
	features.append(QStringLiteral("HAVE_CHAT_LINE_EDIT_MOBILE"));
#endif
#ifdef HAVE_COMPATIBILITY_MODE
	features.append(QStringLiteral("HAVE_COMPATIBILITY_MODE"));
#endif
#ifdef HAVE_EMULATED_BITMAP_CURSOR
	features.append(QStringLiteral("HAVE_EMULATED_BITMAP_CURSOR"));
#endif
#ifdef HAVE_CLIPBOARD_EMULATION
	features.append(QStringLiteral("HAVE_CLIPBOARD_EMULATION"));
#endif
#ifdef HAVE_VIDEO_EXPORT
	features.append(QStringLiteral("HAVE_VIDEO_EXPORT"));
#endif
#ifdef HAVE_QTKEYCHAIN
	features.append(QStringLiteral("HAVE_QTKEYCHAIN"));
#endif
#ifdef SINGLE_MAIN_WINDOW
	features.append(QStringLiteral("SINGLE_MAIN_WINDOW"));
#endif
#ifdef QTCOLORPICKER_STATICALLY_LINKED
	features.append(QStringLiteral("QTCOLORPICKER_STATICALLY_LINKED"));
#endif
#ifdef DP_QT_IO
	features.append(QStringLiteral("DP_QT_IO"));
#endif
#ifdef DP_QT_IO_KARCHIVE
	features.append(QStringLiteral("DP_QT_IO_KARCHIVE"));
#endif
	return features.join(QStringLiteral(", "));
}

QString SystemInfoDialog::boolToString(bool b)
{
	return b ? QStringLiteral("enabled") : QStringLiteral("disabled");
}

void SystemInfoDialog::copyToClipboard()
{
	m_textEdit->selectAll();
	QGuiApplication::clipboard()->setText(m_textEdit->toPlainText());
}

}
