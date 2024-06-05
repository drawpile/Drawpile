// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/canvas_state.h>
}
#include "desktop/dialogs/animationimportdialog.h"
#include "desktop/dialogs/colordialog.h"
#include "desktop/dialogs/startdialog/create.h"
#include "desktop/filewrangler.h"
#include "desktop/main.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/import/animationimporter.h"
#include <QAction>
#include <QApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryFile>
#include <QThreadPool>
#include <QVBoxLayout>
#include <QtColorWidgets/ColorPreview>
#include <functional>

using color_widgets::ColorDialog;
using color_widgets::ColorPreview;
using std::placeholders::_1;
using std::placeholders::_2;

namespace dialogs {

AnimationImportDialog::AnimationImportDialog(int source, QWidget *parent)
	: QDialog(parent)
{
	setModal(true);
	setWindowTitle(tr("Import Animation"));

#ifdef __EMSCRIPTEN__
	resize(800, 300);
#else
	resize(800, 600);
	m_collator.setCaseSensitivity(Qt::CaseInsensitive);
	m_collator.setIgnorePunctuation(true);
	m_collator.setNumericMode(true);
#endif

	QVBoxLayout *layout = new QVBoxLayout;
	setLayout(layout);

	m_tabs = new QTabWidget;
	layout->addWidget(m_tabs);

	QWidget *framesPage = new QWidget;
	int framesIndex = m_tabs->addTab(framesPage, tr("Frames"));
	Q_ASSERT(framesIndex == int(Source::Frames));
	m_tabs->setTabToolTip(
		framesIndex, tr("Import a series of images as timeline frames."));

	QFormLayout *framesLayout = new QFormLayout(framesPage);

#ifdef __EMSCRIPTEN__
	QLabel *framesLabel =
		new QLabel(tr("Importing animation frames is not supported in the web "
					  "browser version of Drawpile."));
	framesLabel->setWordWrap(true);
	framesLayout->addRow(framesLabel);
#else

	m_backgroundPreview = dialogs::startdialog::Create::makeBackgroundPreview(
		dpApp().settings().newCanvasBackColor());
	framesLayout->addRow(tr("Canvas Background:"), m_backgroundPreview);
	connect(
		m_backgroundPreview, &ColorPreview::clicked, this,
		&AnimationImportDialog::showColorPicker);

	m_framesPathsList = new QListWidget;
	m_framesPathsList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_framesPathsList->setDragDropMode(QAbstractItemView::InternalMove);
	framesLayout->addRow(m_framesPathsList);

	QHBoxLayout *framesButtonLayout = new QHBoxLayout;
	framesButtonLayout->addStretch();
	framesLayout->addRow(framesButtonLayout);

	m_addButton = new QPushButton(QIcon::fromTheme("list-add"), tr("Add"));
	framesButtonLayout->addWidget(m_addButton);
	connect(
		m_addButton, &QPushButton::clicked, this,
		&AnimationImportDialog::chooseFramesFiles);

	m_removeButton =
		new QPushButton(QIcon::fromTheme("list-remove"), tr("Remove"));
	framesButtonLayout->addWidget(m_removeButton);
	connect(
		m_removeButton, &QPushButton::clicked, this,
		&AnimationImportDialog::removeSelectedFrames);

	m_sortButton = new QPushButton(QIcon::fromTheme("view-sort"), tr("Sort"));
	framesButtonLayout->addWidget(m_sortButton);

	QMenu *m_sortMenu = new QMenu(m_sortButton);
	m_sortButton->setMenu(m_sortMenu);

	QAction *sortNumericAscending =
		m_sortMenu->addAction(tr("Numeric Ascending"));
	connect(
		sortNumericAscending, &QAction::triggered, this,
		std::bind(&AnimationImportDialog::sortFramesPaths, this, true, true));

	QAction *sortNumericDescending =
		m_sortMenu->addAction(tr("Numeric Descending"));
	connect(
		sortNumericDescending, &QAction::triggered, this,
		std::bind(&AnimationImportDialog::sortFramesPaths, this, false, true));

	QAction *sortAlphabeticAscending =
		m_sortMenu->addAction(tr("Alphabetic Ascending"));
	connect(
		sortAlphabeticAscending, &QAction::triggered, this,
		std::bind(&AnimationImportDialog::sortFramesPaths, this, true, false));

	QAction *sortAlphabeticDescending =
		m_sortMenu->addAction(tr("Alphabetic Descending"));
	connect(
		sortAlphabeticDescending, &QAction::triggered, this,
		std::bind(&AnimationImportDialog::sortFramesPaths, this, false, false));
#endif

	QWidget *layersPage = new QWidget;
	int layersIndex = m_tabs->addTab(layersPage, tr("Layers"));
	Q_ASSERT(layersIndex == int(Source::Layers));
	m_tabs->setTabToolTip(
		layersIndex,
		tr("Import the layers of an ORA or PSD file as timeline frames."));

	QFormLayout *layersForm = new QFormLayout(layersPage);

	QHBoxLayout *pathLayout = new QHBoxLayout;
	layersForm->addRow(tr("File to Import:"), pathLayout);

	m_layersPathEdit = new QLineEdit;
	m_layersPathEdit->setReadOnly(true);
	pathLayout->addWidget(m_layersPathEdit);

	m_chooseButton = new QPushButton(tr("Choose"));
	pathLayout->addWidget(m_chooseButton);
	connect(
		m_chooseButton, &QAbstractButton::clicked, this,
		&AnimationImportDialog::chooseLayersFile);

	QFormLayout *form = new QFormLayout;
	layout->addLayout(form);

	m_holdTime = new QSpinBox;
	m_holdTime->setRange(1, 99);
	m_holdTime->setValue(1);
	//: How many frames each imported key frame gets in the timeline.
	form->addRow(tr("Key frame length:"), m_holdTime);
	updateHoldTimeSuffix(m_holdTime->value());
	connect(
		m_holdTime, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&AnimationImportDialog::updateHoldTimeSuffix);

	m_framerate = new QSpinBox;
	m_framerate->setRange(1, 999);
	m_framerate->setValue(24);
	m_framerate->setSuffix(tr(" FPS"));
	form->addRow(tr("Framerate:"), m_framerate);

	m_buttons = new QDialogButtonBox(QDialogButtonBox::Cancel);
	layout->addWidget(m_buttons);
	connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	m_importButton =
		m_buttons->addButton(tr("Import"), QDialogButtonBox::ActionRole);
	m_importButton->setIcon(QIcon::fromTheme("document-import"));

	if(source >= 0 && source < m_tabs->count()) {
		m_tabs->setCurrentIndex(source);
	}

	connect(
		m_buttons, &QDialogButtonBox::clicked, this,
		&AnimationImportDialog::buttonClicked);
	connect(
		m_layersPathEdit, &QLineEdit::textChanged, this,
		&AnimationImportDialog::updateImportButton);
	connect(
		m_tabs, &QTabWidget::currentChanged, this,
		&AnimationImportDialog::updateImportButton);
#ifndef __EMSCRIPTEN__
	connect(
		m_framesPathsList, &QListWidget::itemSelectionChanged, this,
		&AnimationImportDialog::updateFrameButtons);
	updateFrameButtons();
#endif
	updateImportButton();
}

AnimationImportDialog::~AnimationImportDialog()
{
	if(!isEnabled()) {
		QApplication::restoreOverrideCursor();
	}
}

QString AnimationImportDialog::getStartPageArgumentForSource(int source)
{
	switch(source) {
	case int(Source::Frames):
		return QStringLiteral("import-animation-frames");
	case int(Source::Layers):
		return QStringLiteral("import-animation-layers");
	default:
		qWarning("getStartPageArgumentForSource: unknown source %d", source);
		return QString();
	}
}

#ifndef __EMSCRIPTEN__

void AnimationImportDialog::showColorPicker()
{
	ColorDialog *dlg = dialogs::newDeleteOnCloseColorDialog(
		m_backgroundPreview->color(), this);
	connect(
		dlg, &ColorDialog::colorSelected, m_backgroundPreview,
		&ColorPreview::setColor);
	dlg->show();
}

void AnimationImportDialog::chooseFramesFiles()
{
	QStringList paths = FileWrangler(this).openAnimationFramesImport();
	addFramesPaths(paths);
}

void AnimationImportDialog::removeSelectedFrames()
{
	for(QListWidgetItem *item : m_framesPathsList->selectedItems()) {
		delete m_framesPathsList->takeItem(m_framesPathsList->row(item));
	}
	updateImportButton();
}

void AnimationImportDialog::updateFrameButtons()
{
	m_removeButton->setEnabled(
		m_framesPathsList->selectionModel()->hasSelection());
	m_sortButton->setEnabled(m_framesPathsList->model()->rowCount() != 0);
}

#endif

void AnimationImportDialog::chooseLayersFile()
{
	FileWrangler(this).openAnimationLayersImport(
		std::bind(&AnimationImportDialog::onOpenLayersFile, this, _1, _2));
}

void AnimationImportDialog::updateHoldTimeSuffix(int value)
{
	m_holdTime->setSuffix(tr(" frame(s)", "", value));
}

void AnimationImportDialog::updateImportButton()
{
	bool enabled;
	switch(m_tabs->currentIndex()) {
#ifndef __EMSCRIPTEN__
	case int(Source::Frames):
		enabled = m_framesPathsList->count() != 0;
		break;
#endif
	case int(Source::Layers):
		enabled = !m_layersPathEdit->text().trimmed().isEmpty();
		break;
	default:
		enabled = false;
		break;
	}
	m_importButton->setEnabled(enabled);
}

void AnimationImportDialog::buttonClicked(QAbstractButton *button)
{
	if(button == m_importButton) {
		runImport();
	}
}

void AnimationImportDialog::importFinished(
	const drawdance::CanvasState &canvasState, const QString &error)
{
	if(canvasState.isNull()) {
		QApplication::restoreOverrideCursor();
		QMessageBox::critical(this, tr("Animation Import Error"), error);
		setEnabled(true);
	} else {
		emit canvasStateImported(canvasState);
	}
}

#ifndef __EMSCRIPTEN__

void AnimationImportDialog::sortFramesPaths(bool ascending, bool numeric)
{
	m_ascending = ascending;
	m_collator.setNumericMode(numeric);
	QStringList paths = getFramesPaths();
	m_framesPathsList->clear();
	addFramesPaths(paths);
}

void AnimationImportDialog::addFramesPaths(QStringList &paths)
{
	std::sort(
		paths.begin(), paths.end(), [this](const QString &a, const QString &b) {
			return m_ascending ? m_collator(a, b) : m_collator(b, a);
		});
	m_framesPathsList->addItems(paths);
	updateImportButton();
}

QStringList AnimationImportDialog::getFramesPaths() const
{
	int count = m_framesPathsList->count();
	QStringList paths;
	paths.reserve(count);
	for(int i = 0; i < count; ++i) {
		paths.append(m_framesPathsList->item(i)->text());
	}
	return paths;
}

#endif

void AnimationImportDialog::onOpenLayersFile(
	const QString &path, QTemporaryFile *tempFile)
{
	m_layersPathEdit->setText(path);
	delete m_tempFile;
	m_tempFile = tempFile;
	if(tempFile) {
		tempFile->setParent(this);
	}
}

void AnimationImportDialog::runImport()
{
	if(isEnabled()) {
		impex::AnimationImporter *importer;
		int source = m_tabs->currentIndex();
		int holdTime = m_holdTime->value();
		int framerate = m_framerate->value();
		switch(source) {
#ifndef __EMSCRIPTEN__
		case int(Source::Frames): {
			QColor backgroundColor = m_backgroundPreview->color();
			dpApp().settings().setNewCanvasBackColor(backgroundColor);
			importer = new impex::AnimationFramesImporter(
				getFramesPaths(), backgroundColor, holdTime, framerate);
			break;
		}
#endif
		case int(Source::Layers):
			importer = new impex::AnimationLayersImporter(
				m_tempFile ? m_tempFile->fileName()
						   : m_layersPathEdit->text().trimmed(),
				holdTime, framerate);
			break;
		default:
			qWarning("Unknown source %d", source);
			return;
		}

		connect(
			importer, &impex::AnimationImporter::finished, this,
			&AnimationImportDialog::importFinished);
		QThreadPool::globalInstance()->start(importer);
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		setEnabled(false);
	}
}

}
